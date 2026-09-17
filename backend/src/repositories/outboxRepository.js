export function outboxRepository(db) {
 return {
  record(event,contacts,time) {
   // One event-level sink record, including a durable snapshot of every eligible recipient.
   // The legacy recipient columns describe the sole recipient only when there is exactly one.
   const single=contacts.length===1?contacts[0]:null;
   db.prepare(`INSERT INTO alert_outbox(event_id,user_id,contact_id,contact_name,phone,message_content,status,created_at_ms,recipients_json) VALUES(?,?,?,?,?,?,'RECORDED',?,?)`).run(event.eventId,event.userId,single?.id??'',single?.name??'',single?.phone??'',JSON.stringify({eventId:event.eventId,deviceId:event.deviceId,response:event.response,location:event.location}),time,JSON.stringify(contacts));
  },
 };
}
