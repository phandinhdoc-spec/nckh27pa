function contact(row) {return row?{id:row.id,userId:row.user_id,name:row.name,relationship:row.relationship,phone:row.phone,receiveSos:!!row.receive_sos,isPrimary:!!row.is_primary,callPriority:row.call_priority}:null;}
export function contactRepository(db) {
 return {
  list: user=>db.prepare('SELECT * FROM emergency_contacts WHERE user_id=? ORDER BY call_priority,rowid').all(user).map(contact),
  get: (user,id)=>contact(db.prepare('SELECT * FROM emergency_contacts WHERE user_id=? AND id=?').get(user,id)),
  clearPrimary: user=>db.prepare('UPDATE emergency_contacts SET is_primary=0 WHERE user_id=?').run(user),
  primary: (user,id)=>db.prepare('UPDATE emergency_contacts SET is_primary=1 WHERE user_id=? AND id=?').run(user,id),
  save(user,id,b) {db.prepare(`INSERT INTO emergency_contacts(user_id,id,name,relationship,phone,receive_sos,is_primary,call_priority) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(user_id,id) DO UPDATE SET name=excluded.name,relationship=excluded.relationship,phone=excluded.phone,receive_sos=excluded.receive_sos,is_primary=excluded.is_primary,call_priority=excluded.call_priority`).run(user,id,b.name,b.relationship,b.phone,+b.receiveSos,+b.isPrimary,b.callPriority);},
  delete: (user,id)=>db.prepare('DELETE FROM emergency_contacts WHERE user_id=? AND id=?').run(user,id),
 };
}
