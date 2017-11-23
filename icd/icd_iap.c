#include <sys/types.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <gconf/gconf-client.h>
#include <osso-ic-dbus.h>
#include <osso-ic-gconf.h>
#include "icd_iap.h"
#include "icd_network_api.h"
#include "icd_log.h"
#include "icd_context.h"
#include "icd_policy_api.h"
#include "icd_idle_timer.h"
#include "icd_request.h"
#include "icd_script.h"
#include "network_api.h"
#include "icd_gconf.h"
#include "icd_osso_ic.h"
#include "icd_status.h"
#include "icd_srv_provider.h"
#include "icd_dbus_api.h"

/** names for the different states */
const gchar *icd_iap_state_names[ICD_IAP_MAX_STATES] = {
  "ICD_IAP_STATE_DISCONNECTED",
  "ICD_IAP_STATE_SCRIPT_PRE_UP",
  "ICD_IAP_STATE_LINK_UP",
  "ICD_IAP_STATE_LINK_POST_UP",
  "ICD_IAP_STATE_IP_UP",
  "ICD_IAP_STATE_SRV_UP",
  "ICD_IAP_STATE_SCRIPT_POST_UP",
  "ICD_IAP_STATE_SAVING",
  "ICD_IAP_STATE_CONNECTED",
  "ICD_IAP_STATE_CONNECTED_DOWN",
  "ICD_IAP_STATE_SRV_DOWN",
  "ICD_IAP_STATE_IP_DOWN",
  "ICD_IAP_STATE_IP_RESTART_SCRIPTS",
  "ICD_IAP_STATE_LINK_PRE_DOWN",
  "ICD_IAP_STATE_LINK_PRE_RESTART_SCRIPTS",
  "ICD_IAP_STATE_LINK_DOWN",
  "ICD_IAP_STATE_LINK_RESTART_SCRIPTS",
  "ICD_IAP_STATE_SCRIPT_POST_DOWN"
};

/** names for status codes */
static const gchar *icd_iap_status_names[] =
 {
  "ICD_IAP_CREATED",
  "ICD_IAP_DISCONNECTED",
  "ICD_IAP_BUSY",
  "ICD_IAP_FAILED"
};

/** names for network module layers */
static const gchar *icd_iap_layer_names[] = {
  "ICD_NW_LAYER_NONE",
  "ICD_NW_LAYER_LINK",
  "ICD_NW_LAYER_LINK_POST",
  "ICD_NW_LAYER_IP",
  "ICD_NW_LAYER_SERVICE",
  "ICD_NW_LAYER_ALL"
};

/**
 * @brief Helper function for comparing two strings where a NULL string is equal
 * to another NULL string
 *
 * @param a string A
 * @param b string B
 *
 * @return TRUE if equal, FALSE if unequal
 *
 */
inline static gboolean
string_equal(const char *a, const char *b)
{
  if (!a)
    return !b;

  if (b)
    return !strcmp(a, b);

  return FALSE;
}

/**
 * @brief Iterate over all active IAPs
 *
 * @param fn function to call for each IAP
 * @param user_data user data to pass to the iterator function
 *
 * @return the IAP struct where fn returns FALSE, NULL otherwise or on error
 *
 */
struct icd_iap *
icd_iap_foreach(icd_iap_foreach_fn fn, gpointer user_data)
{
  struct icd_context *icd_ctx;
  GSList *l;

  if (!fn)
  {
    ILOG_ERR("iap iterator function NULL");
    return NULL;
  }

  icd_ctx = icd_context_get();
  l = icd_ctx->request_list;

  if (!l)
    return NULL;

  for (l = icd_ctx->request_list; l; l = l->next)
  {
    GSList *iaps;
    struct icd_request *request = (struct icd_request *)l->data;

    if (!request)
    {
      ILOG_ERR("request in list is NULL");
      continue;
    }

    iaps = request->try_iaps;

    if (!iaps)
    {
      ILOG_DEBUG("request %p has no IAPs", request);
      continue;
    }

    if (iaps->data)
    {
      struct icd_iap *iap = (struct icd_iap *)iaps->data;

      if (!fn(iap, user_data))
        return iap;
    }
    else
      ILOG_ERR("request %p has NULL iap in list", request);
  }

  return NULL;
}

/**
 * @brief Restart a network module by disconnecting network modules including
 * the requested layer. When the requested layer has been disconnected,
 * reconnect starting from the requested layer.
 *
 * @param iap the IAP
 * @param restart_layer the layer which is to be disconnected
 *
 */
void
icd_iap_restart(struct icd_iap *iap, enum icd_nw_layer restart_layer)
{
  if (iap->state <= ICD_IAP_STATE_CONNECTED ||
      iap->state == ICD_IAP_STATE_IP_RESTART_SCRIPTS ||
      iap->state == ICD_IAP_STATE_LINK_PRE_RESTART_SCRIPTS ||
      iap->state == ICD_IAP_STATE_LINK_RESTART_SCRIPTS)

  {
    ILOG_INFO("restarting iap %p layer %s", iap,
              icd_iap_layer_names[restart_layer]);

    iap->restart_layer = restart_layer;
    iap->restart_state = iap->state;
    icd_iap_disconnect(iap, NULL);
  }
  else
    ILOG_INFO("ignored restart for iap %p since already disconnecting", iap);
}

/**
 * @brief Disconnect callback function for all IAP network _down functions
 *
 * @param status the status of the _down function, ignored mostly for now
 * @param cb_token the IAP
 *
 */
static void
icd_iap_disconnect_cb(const enum icd_nw_status status, const gpointer cb_token)
{
  struct icd_iap *iap = (struct icd_iap *)cb_token;
  enum icd_iap_state state = iap->state;

  if (state == ICD_IAP_STATE_SRV_DOWN || state == ICD_IAP_STATE_IP_DOWN ||
      state == ICD_IAP_STATE_LINK_PRE_DOWN || state == ICD_IAP_STATE_LINK_DOWN)
  {
    ILOG_DEBUG("IAP %p in state %s disconnected", iap,
               icd_iap_state_names[iap->state]);
    icd_iap_disconnect_module(iap);
  }
  else
    ILOG_ERR("IAP disconnect cb called in state %s",
             icd_iap_state_names[iap->state]);
}

/**
 * @brief  Pre-down script callback to remove script pid from list.
 *
 * @param pid the process id of the script that exited
 * @param exit_value exit value of the script or -1 on timeout
 * @param user_data user data (IAP)
 *
 */
static void
icd_iap_pre_down_script_done(pid_t pid, gint exit_value, gpointer user_data)
{
  struct icd_iap *iap = (struct icd_iap *)user_data;

  iap->script_pids = g_slist_remove(iap->script_pids, GINT_TO_POINTER(pid));

  if (iap->script_pids)
    ILOG_INFO("still more pre-down scripts to come, waiting");
  else
    icd_iap_disconnect_module(iap);
}

/**
 * @brief Helper function for pre down script to check wheter there is another
 * connected IAP
 *
 * @param iap IAP to examine
 * @param user_data IAP that is going down
 *
 * @return TRUE to continue iterating, FALSE to stop and return the IAP
 *
 */
static gboolean
icd_iap_check_connected(struct icd_iap *iap, gpointer user_data)
{
  if (iap == user_data)
    return TRUE;

  return iap->state != ICD_IAP_STATE_CONNECTED;
}

/**
 * @brief Disconnect callback for the service provider module
 *
 * @param status status of the disconnect, ignored for now
 * @param disconnect_cb_token token passed to the disconnect function
 *
 */
static void
icd_iap_srv_disconnect_cb(enum icd_srv_status status,
                          gpointer disconnect_cb_token)
{
  icd_iap_disconnect_module((struct icd_iap *)disconnect_cb_token);
}

/**
 * @brief Start disconnecting the current connecting module if it has not yet
 * called it's callback. Set the state to _down so that the IAP cannot be
 * disconnected again.
 *
 * @param iap IAP
 * @param err_str NULL if the network was disconnected normally or any
 * ICD_DBUS_ERROR_* from osso-ic-dbus.h on error
 *
 */
void
icd_iap_disconnect(struct icd_iap *iap, const gchar *err_str)
{
  ILOG_INFO("disconnect requested for IAP %p in state %s, %s", iap,
            icd_iap_state_names[iap->state], err_str ? err_str : "no error");

  icd_idle_timer_unset(iap);

  switch ( iap->state )
  {
    case ICD_IAP_STATE_DISCONNECTED:
      ILOG_WARN("disconnect requested for already disconnected IAP %p", iap);
      break;
    case ICD_IAP_STATE_SCRIPT_PRE_UP:
      ILOG_INFO("disconnect requested for IAP %p in pre_up", iap);
      iap->err_str = g_strdup(err_str);
      icd_iap_disconnect_module(iap);
      break;
    case ICD_IAP_STATE_LINK_UP:
    {
      struct icd_network_module *module = NULL;

      iap->err_str = g_strdup(err_str);

      if (iap->current_module)
      {
        module = (struct icd_network_module *)iap->current_module->data;

        if (module && module->nw.link_down )
        {
          ILOG_INFO("calling link_down function in last module '%s' when disconnecting",
                    module->name);

          iap->state = ICD_IAP_STATE_LINK_DOWN;
          module->nw.link_down(iap->connection.network_type,
                               iap->connection.network_attrs,
                               iap->connection.network_id, NULL,
                               icd_iap_disconnect_cb, iap, &module->nw.private);
          return;
        }
      }

      ILOG_INFO("no link_down function in last tried module '%s' when disconnecting",
                module ? module->name : NULL);

      icd_iap_disconnect_module(iap);
      break;
    }
    case ICD_IAP_STATE_LINK_POST_UP:
    {
      struct icd_network_module *module = NULL;

      iap->err_str = g_strdup(err_str);

      if (iap->current_module)
      {
        module = (struct icd_network_module *)iap->current_module->data;

        if (module && module->nw.link_pre_down)
        {
          ILOG_INFO("calling link_pre_down function in last module '%s' when disconnecting",
                    module->name);

          iap->state = ICD_IAP_STATE_LINK_PRE_DOWN;
          module->nw.link_pre_down(iap->connection.network_type,
                                   iap->connection.network_attrs,
                                   iap->connection.network_id,
                                   iap->interface_name,
                                   icd_iap_disconnect_cb, iap,
                                   &module->nw.private);
          return;
        }
      }

      ILOG_INFO("no link_pre_down function in last tried module '%s' when disconnecting",
                module ? module->name : NULL);

      icd_iap_disconnect_module(iap);
      break;
    }
    case ICD_IAP_STATE_IP_UP:
    {
      struct icd_network_module *module = NULL;

      iap->err_str = g_strdup(err_str);

      if (iap->current_module)
      {
        module = (struct icd_network_module *)iap->current_module->data;

        if (module && module->nw.ip_down)
        {
          ILOG_INFO("calling ip_down function in last tried module '%s' when disconnecting",
                    module->name);

          iap->state = ICD_IAP_STATE_IP_DOWN;
          module->nw.ip_down(
                iap->connection.network_type,
                iap->connection.network_attrs,
                iap->connection.network_id,
                iap->interface_name,
                icd_iap_disconnect_cb, iap,
                &module->nw.private);
          return;
        }
      }

      ILOG_INFO("no ip_down function in last tried module '%s' when disconnecting",
                module ? module->name : NULL);

      icd_iap_disconnect_module(iap);
      break;
    }
    case ICD_IAP_STATE_SRV_UP:
      iap->err_str = g_strdup(err_str);

      if (iap->limited_conn)
      {
        iap->limited_conn = FALSE;
        icd_status_limited_conn(iap, NULL, NULL);
      }

      if (icd_srv_provider_disconnect(iap, icd_iap_srv_disconnect_cb, iap))
        ILOG_INFO("called srv disconnect function when disconnecting");
      else
      {
        ILOG_INFO("no srv module to call when disconnecting");
        icd_iap_disconnect_module(iap);
      }
      break;
    case ICD_IAP_STATE_SAVING:
      icd_osso_ui_send_save_cancel(iap->save_dlg);
    case ICD_IAP_STATE_SCRIPT_POST_UP:
    case ICD_IAP_STATE_CONNECTED:
    {
      gchar *iap_id = NULL;
      gboolean remove_proxies;
      GSList *script_env;

      ILOG_INFO("disconnect requested for IAP %p", iap);
      iap->err_str = g_strdup(err_str);
      iap->state = ICD_IAP_STATE_CONNECTED_DOWN;

      while (iap->script_pids)
      {
        pid_t pid = (pid_t)(iap->script_pids->data);
        ILOG_DEBUG("requesting cancellation of script pid %d", pid);
        icd_script_cancel(pid);
        iap->script_pids = g_slist_delete_link(iap->script_pids,
                                               iap->script_pids);
      }

      if (iap->id && !iap->id_is_local)
        iap_id = gconf_escape_key(iap->id, -1);

      remove_proxies = !icd_iap_foreach(icd_iap_check_connected, iap);
      script_env = iap->script_env;

      if (script_env)
      {
        for (; script_env; script_env = script_env->next)
        {
          if (script_env->data)
          {
            const struct icd_iap_env *env = script_env->data;
            pid_t pid = icd_script_pre_down(iap->interface_name, iap_id,
                                            iap->connection.network_type,
                                            remove_proxies,
                                            env,
                                            icd_iap_pre_down_script_done,
                                            iap);
            iap->script_pids = g_slist_prepend(iap->script_pids,
                                               (gpointer)(intptr_t)pid);
          }

        }
      }
      else
      {
        ILOG_INFO("no env vars for pre-down script");

        pid_t pid = icd_script_pre_down(iap->interface_name, iap_id,
                                        iap->connection.network_type,
                                        remove_proxies, NULL,
                                        icd_iap_pre_down_script_done,
                                        iap);
        iap->script_pids = g_slist_prepend(iap->script_pids,
                                           (gpointer)(intptr_t)pid);
      }

      g_free(iap_id);
      break;
    }
    default:
      ILOG_INFO("disconnect requested for already disconnecting IAP %p", iap);
      break;
  }
}

struct icd_iap *
icd_iap_find(const gchar *network_type, const guint network_attrs,
             const gchar *network_id)
{
  GSList *l;

  for (l = icd_context_get()->request_list; l; l = l->next)
  {
    struct icd_request *request = (struct icd_request *)l->data;

    if (request)
    {
      if (request->try_iaps)
      {
        struct icd_iap *iap = (struct icd_iap *)request->try_iaps->data;

        if (((network_attrs & ICD_NW_ATTR_LOCALMASK) ==
             (iap->connection.network_attrs & ICD_NW_ATTR_LOCALMASK) ||
             (iap->connection.network_attrs & ICD_NW_ATTR_IAPNAME) ==
             (network_attrs & ICD_NW_ATTR_IAPNAME)) &&
            string_equal(network_type, iap->connection.network_type) &&
            string_equal(network_id, iap->connection.network_id))
        {
          ILOG_DEBUG("IAP for %s/%0x/%s found", network_type, network_attrs,
                     network_id);
          return iap;
        }
      }
      else
        ILOG_ERR("request %p does not have iaps", request);
    }
    else
      ILOG_ERR("request in request list is NULL");
  }

  return NULL;
}

static void
icd_iap_do_callback(enum icd_iap_status status, struct icd_iap *iap)
{
  ILOG_INFO("IAP status is %s", icd_iap_status_names[status]);

  iap->request_cb(status, iap, iap->request_cb_user_data);
}

static void
icd_iap_modules_reset(struct icd_iap *iap)
{
  iap->current_module = NULL;
}

static void
icd_iap_has_connected(struct icd_iap *iap)
{
  iap->state = ICD_IAP_STATE_CONNECTED;
  icd_iap_modules_reset(iap);
  icd_idle_timer_set(iap);
  icd_iap_do_callback(ICD_IAP_CREATED, iap);
}

gboolean
icd_iap_rename(struct icd_iap *iap, const gchar *name)
{
  gboolean rv = FALSE;

  if (iap->id && !iap->id_is_local)
  {
    rv = icd_gconf_rename(iap->id, name);

    ILOG_INFO("IAP %p settings '%s' renamed to '%s'", iap, iap->id, name);
  }
  else
    ILOG_ERR("iap id is unset when renaming");

  if (iap->state == ICD_IAP_STATE_SAVING)
    icd_iap_has_connected(iap);

  return rv;
}
