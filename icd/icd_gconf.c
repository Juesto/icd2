#include <string.h>
#include <gconf/gconf-client.h>
#include <osso-ic-gconf.h>
#include "icd_log.h"
#include "icd_gconf.h"
#include "icd_context.h"
#include "icd_scan.h"


#define ICD_GCONF_IAP_TYPE            "type"
#define ICD_GCONF_IAP_NAME            "name"
#define ICD_GCONF_IAP_IS_TEMPORARY    "temporary"
#define ICD_GCONF_AGGRESSIVE_SCANNING "aggressive_scanning"

static void
icd_gconf_check_error(GError **error)
{
  if (error && *error)
  {
    ILOG_ERR("icd gconf error: %s", (*error)->message);
    g_clear_error(error);
    *error = NULL;
  }
}

gboolean
icd_gconf_get_iap_bool(const char *iap_name, const char *key_name, gboolean def)
{
  GConfClient *gconf = gconf_client_get_default();
  gchar *key;
  GConfValue *val;
  gboolean rv = def;
  GError *err = NULL;

  if (iap_name)
  {
    gchar *s = gconf_escape_key(iap_name, -1);
    key = g_strdup_printf(ICD_GCONF_PATH  "/%s/%s", s, key_name);
    g_free(s);
  }
  else
    key = g_strdup_printf(ICD_GCONF_PATH "/%s", key_name);

  val = gconf_client_get(gconf, key, &err);
  g_free(key);
  icd_gconf_check_error(&err);

  if (val)
  {
    if (G_VALUE_HOLDS_BOOLEAN(val))
      rv = gconf_value_get_bool(val);

    gconf_value_free(val);
  }

  g_object_unref(gconf);

  return rv;
}

gboolean
icd_gconf_is_temporary(const gchar *settings_name)
{
  if (!settings_name)
    return FALSE;

  if (!icd_gconf_get_iap_bool(settings_name, ICD_GCONF_IAP_IS_TEMPORARY, FALSE))
  {
    if (!strncmp(settings_name, "[Easy", 5))
      ILOG_DEBUG("settings is temp IAP because of '[Easy' name prefix");

    return FALSE;
  }
  else
    ILOG_DEBUG("setting is temp IAP because of 'temporary' key");

  return TRUE;
}

static gboolean
icd_gconf_remove_dir(const gchar *settings_name)
{
  GConfClient *gconf = gconf_client_get_default();
  gboolean rv = FALSE;
  GError *err = NULL;

  if (icd_gconf_is_temporary(settings_name))
  {
    gchar *s = gconf_escape_key(settings_name, -1);
    gchar *key = g_strdup_printf(ICD_GCONF_PATH "/%s", s);

    g_free(s);
    gconf_client_recursive_unset(gconf, key, GCONF_UNSET_INCLUDING_SCHEMA_NAMES,
                                 &err);

    if (err)
      icd_gconf_check_error(&err);
    else
    {
      ILOG_DEBUG("icd gconf removed '%s' from gconf", key);
      rv = TRUE;
    }

    g_free(key);
  }

  g_object_unref(gconf);

  return rv;
}

gboolean
icd_gconf_remove_temporary(const gchar *settings_name)
{
  GConfClient *gconf = gconf_client_get_default();
  GError *err = NULL;
  gboolean rv = FALSE;
  GSList *l;

  if (settings_name)
    return icd_gconf_remove_dir(settings_name);

  l = gconf_client_all_dirs(gconf, ICD_GCONF_PATH, &err);

  if (err)
  {
    icd_gconf_check_error(&err);
    return FALSE;
  }

  while (l)
  {
    const gchar *p = g_strrstr((const gchar *)l->data, "/");

    if (p)
    {
      gchar *dir = gconf_unescape_key(p + 1, -1);

      if (icd_gconf_remove_dir(dir))
        rv = TRUE;

      g_free(dir);
    }

    g_free(l->data);
    l = g_slist_remove_link(l, l);
  }

  return rv;
}

static void
icd_gconf_notify(GConfClient *gconf_client, guint connection_id,
                 GConfEntry *entry, gpointer user_data)
{
  size_t len = strlen(ICD_GCONF_PATH "/");
  const char *key = gconf_entry_get_key(entry);

  if (!strncmp(key, ICD_GCONF_PATH "/", len) && !strchr(&key[len], '/') &&
      !gconf_entry_get_value(entry))
  {
    gchar *iap_name = gconf_unescape_key(&key[len], -1);

    ILOG_DEBUG("IAP (%s) deletion detected, checking cache", iap_name);

    icd_scan_cache_remove_iap(iap_name);
    g_free(iap_name);
  }
}

gboolean
icd_gconf_add_notify(void)
{
  struct icd_context *icd_ctx = icd_context_get();
  GConfClient *gconf = gconf_client_get_default();

  icd_ctx->iap_deletion_notify =
      gconf_client_notify_add(gconf, ICD_GCONF_PATH, icd_gconf_notify, NULL,
                              NULL, NULL);
  g_object_unref(gconf);

  return icd_ctx->iap_deletion_notify != 0;
}
