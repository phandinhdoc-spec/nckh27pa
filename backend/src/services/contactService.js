import {randomUUID} from 'node:crypto';
import * as v from './validation.js';
import {ApiError,invalid,missing,conflict} from './errors.js';
import {transaction} from '../repositories/db.js';
import {contactRepository} from '../repositories/contactRepository.js';
export function uuid(id) {if(typeof id!=='string'||!/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(id)) invalid('Invalid contact UUID v4');return id;}
function validate(b) {
 v.string(b.name,'name',100);v.string(b.relationship,'relationship');v.boolean(b.receiveSos,'receiveSos');v.boolean(b.isPrimary,'isPrimary');
 if(typeof b.phone!=='string') invalid('Invalid phone type');
 const phone=b.phone.replace(/[\s.()\-]/g,'');
 if(!/^(\+84[35789][0-9]{8}|0[35789][0-9]{8})$/.test(phone)) throw new ApiError(400,'INVALID_PHONE_NUMBER','Invalid Vietnamese phone number');
 return {name:b.name.trim(),relationship:b.relationship,phone,receiveSos:b.receiveSos,isPrimary:b.isPrimary};
}
export function contactService({db}) {
 const repo=contactRepository(db);
 return {
  execute(method,user,id,action,b) {
   v.string(user,'userId');if(id!==undefined)uuid(id);
   if(method==='GET') return {data:{userId:user,contacts:repo.list(user)}};
   return transaction(db,()=>{
    if(method==='POST'&&!id) {
     const cid=b.id===undefined?randomUUID():uuid(b.id);
     const existing=repo.get(user,cid);if(existing)return {status:200,data:{contact:existing}};
     const c=validate(b);c.isPrimary=repo.list(user).length===0||c.isPrimary;
     if(c.isPrimary)repo.clearPrimary(user);repo.save(user,cid,c);
     return {status:201,data:{contact:repo.get(user,cid)}};
    }
    const current=repo.get(user,id);if(!current)missing();
    if(method==='DELETE') {
     const list=repo.list(user);if(list.length<=1)conflict('Cannot delete the last emergency contact','CANNOT_DELETE_LAST_CONTACT');
     repo.delete(user,id);if(current.isPrimary)repo.primary(user,list.find(c=>c.id!==id).id);
     return {data:{deletedId:id,remainingCount:list.length-1}};
    }
    if(action==='primary') {repo.clearPrimary(user);repo.primary(user,id);return {data:{primaryContactId:id,updated:true}};}
    if(action==='toggle-sos') {repo.save(user,id,{...current,receiveSos:!current.receiveSos});return {data:{id,receiveSos:!current.receiveSos}};}
    const updated=validate(b);updated.isPrimary=updated.isPrimary||current.isPrimary;
    if(updated.isPrimary)repo.clearPrimary(user);repo.save(user,id,updated);
    return {data:{contact:repo.get(user,id)}};
   });
  },
 };
}
