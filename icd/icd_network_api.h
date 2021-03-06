#ifndef ICD_NETWORK_API_H
#define ICD_NETWORK_API_H

#include <glib.h>

#include "network_api.h"
#include "icd_context.h"

struct icd_network_module {
  void *handle;
  gchar *name;
  GSList *network_types;
  GSList *pid_list;

  guint scope;
  gboolean scan_progress;
  gint scan_timeout_rescan;
  GSList *scan_timeout_list;
  GHashTable *scan_cache_table;
  GSList *scan_listener_list;

  struct icd_nw_api nw;
};

/**
 * @brief Network api callback for going through every network module
 *
 * @param module the network module
 * @param user_data user data passed to #icd_network_api_foreach_module
 *
 * @return if TRUE the callback will be called again with the next module; if
 * FALSE iteration is stopped
 *
 */
typedef gboolean
(*icd_network_api_foreach_module_fn) (struct icd_network_module* module,
                                      gpointer user_data);

gboolean icd_network_api_has_type (struct icd_network_module *module,
                                   const gchar *type);
struct icd_network_module *
icd_network_api_foreach_module (struct icd_context *icd_ctx,
                                icd_network_api_foreach_module_fn foreach_fn,
                                gpointer user_data);
gboolean icd_network_api_notify_pid (struct icd_context *icd_ctx,
                                     const pid_t pid,
                                     const gint exit_value);
gboolean icd_network_api_load_modules (struct icd_context *icd_ctx);
void icd_network_api_unload_modules (struct icd_context *icd_ctx);

#endif
