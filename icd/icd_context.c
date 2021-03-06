#include "icd_context.h"

/**
 * @brief the global ICd context
 */
static struct icd_context icd_ctx;

/**
 * @brief Initialize context
 */
gboolean
icd_context_init(void)
{
  icd_ctx.daemon = FALSE;
  icd_ctx.type_to_module = NULL;
  icd_ctx.nw_module_list = NULL;
  icd_ctx.main_loop = g_main_loop_new(NULL, FALSE);

  return TRUE;
}

/**
 * @brief return the global context
 * @return pointer to #icd_ctx
 */
struct icd_context *
icd_context_get(void)
{
  return &icd_ctx;
}

/**
 * @brief start the main loop
 */
void
icd_context_run(void)
{
  g_main_loop_run(icd_ctx.main_loop);
}

/**
 * @brief stop running the main loop
 */
void
icd_context_stop(void)
{
  g_main_loop_quit(icd_ctx.main_loop);
}

/**
 * @brief destroy context
 */
void
icd_context_destroy(void)
{
}
