#include "ministompd.h"

// Creates a subscription for the given queue.
// Does not take ownership of the queue or connection.
// Takes ownership of client_id and server_id.
subscription *subscription_new(queue *queue, connection *connection, const bytestring *client_id, const bytestring *server_id, sub_ack_type ack_type)
{
  subscription *sub = xmalloc(sizeof(subscription));

  sub->queue       = queue;
  sub->connection  = connection;
  sub->client_id   = client_id;
  sub->server_id   = server_id;
  sub->ack_type    = ack_type;
  sub->deliveries  = hash_new(16);
  sub->next_seqnum = 0;
//  sub->last_qlid = 0;

  return sub;
}

void subscription_deliver(subscription *s, frame *f)
{
  // Get frame headers
  headerbundle *hb = frame_get_headerbundle(f);

  // Get message's 'message-id' header
  const bytestring *msgid = headerbundle_get_header_value_by_str(hb, "message-id");
  assert(msgid != NULL);

  // Create 'delivery' record
  struct delivery *d = xmalloc(sizeof(struct delivery));
  d->frame  = f;
  d->seqnum = s->next_seqnum++;
  d->status = DEL_STATUS_WRITE;
  if (clock_gettime(CLOCK_MONOTONIC, &d->createtime))
    abort();  // Couldn't get time

  // Generate 'ack' header
  int subid_length = bytestring_get_length(s->server_id);
  int msgid_length = bytestring_get_length(msgid);
  int total_length = subid_length + 1 + msgid_length;
  bytestring *ack = bytestring_new(total_length);
  bytestring_append_bytestring(ack, s->server_id);
  bytestring_append_byte(ack, '/');
  bytestring_append_bytestring(ack, msgid);

  // Build local headers
  headerbundle *local_headers;
  local_headers = headerbundle_new();
  headerbundle_append_header(local_headers, bytestring_new_from_string("ack"), ack);

  // Send to frame serializer
  frameserializer *fs = s->connection->frameserializer;
  frameserializer_enqueue_frame(fs, f, local_headers);
}

void subscription_pump(subscription *s)
{
  
}

void subscription_dump(subscription *s)
{
  printf("== subscription %p ==\n", s);
  printf("queue: ");
  bytestring_dump(s->queue->name);
  printf("client-side id: ");
  bytestring_dump(s->client_id);
  printf("server-side id: ");
  bytestring_dump(s->server_id);

  return;
}

void subscription_free(subscription *sub)
{
  bytestring_free((bytestring *) sub->client_id);
  bytestring_free((bytestring *) sub->server_id);
  xfree(sub);
}

