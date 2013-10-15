#ifndef MINISTOMPD_TYPES_H
#define MINISTOMPD_TYPES_H

struct queue;
typedef struct queue queue;

struct queueconfig;
typedef struct queueconfig queueconfig;

struct storage;
typedef struct storage storage;

// *** Queue ***
struct queue
{
  bytestring        *name;
  storage           *storage;
  const queueconfig *config;
};

// *** Storage ***

typedef enum
{
  STORAGE_TYPE_MEMORY     = 0,
  STORAGE_TYPE_SERVERINFO = 1
} storage_type;

struct storage
{
  storage_type type;
  queue       *queue;
  union
  {
    struct storage_memory     *memory;
//    struct storage_serverinfo serverinfo;
  } u;
};

// *** Queueconfig ***
typedef enum
{
  QC_DIST_SINGLE,
  QC_DIST_BROADCAST
} qc_distribution;

typedef enum
{
  QC_FULL_ERROR,
  QC_FULL_DROP_OLDEST,
  QC_FULL_DROP_NEWEST
} qc_full_action;

typedef enum
{
  QC_REJECT_DROP,
  QC_REJECT_REDIRECT
} qc_reject_action;

struct queueconfig
{
  qc_distribution  distribution;

  storage_type     storage_type;
  bytestring_list *storage_args;

  int              size_max;
  qc_full_action   full_action;

  int              age_max;
  qc_reject_action retire_action;

  int              nack_max;
  qc_reject_action nack_action;
};

#endif
