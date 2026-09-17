import {contactRepository} from '../repositories/contactRepository.js';
import {outboxRepository} from '../repositories/outboxRepository.js';
export function alertOutboxService(db) {
 const contacts=contactRepository(db),outbox=outboxRepository(db);
 return {record(event,time) {const recipients=contacts.list(event.userId).filter(c=>c.receiveSos);outbox.record(event,recipients,time);return recipients.length;}};
}
