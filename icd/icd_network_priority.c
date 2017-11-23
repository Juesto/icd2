#include <string.h>
#include <gconf/gconf-client.h>
#include <osso-ic-gconf.h>
#include "icd_log.h"
#include "icd_network_priority.h"
#include "network_api.h"

/**  preferred service network priority */
#define ICD_NW_PRIO_SRV_PREF   500

/** WLAN network prefix */
#define ICD_NW_TYPE_WLAN   "WLAN_"

/** WLAN priority */
#define ICD_NW_PRIO_WLAN   60

/** WiMAX network */
#define ICD_NW_TYPE_WIMAX   "WIMAX"

/** WiMax priority */
#define ICD_NW_PRIO_WIMAX   50

/** GPRS network */
#define ICD_NW_TYPE_GPRS   "GPRS"

/** GPRS priority */
#define ICD_NW_PRIO_GPRS   45

/** GSM GPRS network */
#define ICD_NW_TYPE_DUN_GSM_PS   "DUN_GSM_PS"

/** CDMA packet data network */
#define ICD_NW_TYPE_DUN_CDMA_PSD   "DUN_CDMA_PSD"

/**GSM/CDMA packet data priority */
#define ICD_NW_PRIO_DUN_PS   40

/** GSM circuit switched network */
#define ICD_NW_TYPE_DUN_GSM_CS   "DUN_GSM_CS"

/** CDMA circuit switched network */
#define ICD_NW_TYPE_DUN_CDMA_CSD   "DUN_CDMA_CSD"

/** CDMA Quick Net connect network */
#define ICD_NW_TYPE_DUN_CDMA_QNC   "DUN_CDMA_QNC"

/** GSM/CDMA circuit switched priority */
#define ICD_NW_PRIO_DUN_CS   30

/** What is the highest normal (not preferred) priority */
#define ICD_NW_PRIO_HIGHEST   ICD_NW_PRIO_WLAN

/** The saved IAP priority is made higher */
#define ICD_NW_PRIO_SAVED_BOOSTER_VALUE   100

/** preferred service id */
#define PREFERRED_SERVICE_ID ICD_GCONF_SETTINGS "/srv_provider/preferred_id"

/** preferred service type */
#define PREFERRED_SERVICE_TYPE ICD_GCONF_SETTINGS "/srv_provider/preferred_type"

/** preferred service id */
static gchar *preferred_id = NULL;

/** preferred service type */
static gchar *preferred_type = NULL;

/**
 * @brief (Re)set preferred service type and id
 *
 */
void
icd_network_priority_pref_init (void)
{
  GConfClient *gconf = gconf_client_get_default();

  g_free(preferred_id);
  preferred_id = gconf_client_get_string(gconf, PREFERRED_SERVICE_ID, NULL);

  g_free(preferred_type);
  preferred_type = gconf_client_get_string(gconf,
                                           PREFERRED_SERVICE_TYPE, NULL);

  ILOG_DEBUG("preferred srv type '%s', srv id '%s'", preferred_type,
             preferred_id);
}
