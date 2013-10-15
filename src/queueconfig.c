#include "ministompd.h"

queueconfig *queueconfig_new(void)
{
  queueconfig *qc = xmalloc(sizeof(queueconfig));

  qc->distribution  = QC_DIST_SINGLE;

  qc->storage_type  = STORAGE_TYPE_MEMORY;
  qc->storage_args  = NULL;

  qc->size_max      = DEFAULT_QUEUE_SIZE_MAX;
  qc->full_action   = QC_FULL_ERROR;

  qc->age_max       = 0;
  qc->retire_action = QC_REJECT_DROP;

  qc->nack_max      = DEFAULT_QUEUE_NACK_MAX;
  qc->nack_action   = QC_REJECT_DROP;

  return qc;
}

void queueconfig_free(queueconfig *qc)
{
  xfree(qc);
}

