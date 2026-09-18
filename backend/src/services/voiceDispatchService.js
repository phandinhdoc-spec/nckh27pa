import { ApiError, missing } from './errors.js';
import { buildVoiceTwiml, validateTwilioSignature } from '../providers/twilioVoiceProvider.js';
import { transportStatusService } from './transportStatusService.js';

const TERMINAL = new Set(['busy','no-answer','failed','completed','canceled']);
function attempt(row) {
  return {
    eventId: row.event_id, contactId: row.contact_id, contactName: row.contact_name,
    phone: row.phone, callPriority: row.call_priority, attempt: row.attempt,
    status: row.status, providerCallId: row.provider_call_id,
  };
}

export function voiceDispatchService({ db, config, now, voiceProvider }) {
  const provider = voiceProvider ?? { name: 'DISABLED', configured: false, createCall: () => null };
  const transport = transportStatusService({db,now});

  function updateCallResult(request, result, error) {
    if (error) {
      const changed = db.prepare("UPDATE voice_attempts SET status='failed',updated_at_ms=? WHERE event_id=? AND contact_id=? AND attempt=? AND status='QUEUED'")
        .run(now(), request.eventId, request.contactId, request.attempt).changes;
      if (changed) launchNext(request.eventId);
      return;
    }
    db.prepare("UPDATE voice_attempts SET status='CALLING',provider_call_id=?,updated_at_ms=? WHERE event_id=? AND contact_id=? AND attempt=? AND status='QUEUED'")
      .run(result?.providerCallId ?? null, now(), request.eventId, request.contactId, request.attempt);
    db.prepare("UPDATE voice_dispatches SET status='CALLING',updated_at_ms=? WHERE event_id=? AND acknowledged=0")
      .run(now(), request.eventId);
  }

  function launch(request) {
    try {
      const result = provider.createCall(request);
      if (result && typeof result.then === 'function') result.then(value=>updateCallResult(request,value)).catch(error=>updateCallResult(request,null,error));
      else updateCallResult(request,result);
    } catch (error) { updateCallResult(request,null,error); }
  }

  function contactsFor(eventId) {
    const outbox = db.prepare('SELECT recipients_json FROM alert_outbox WHERE event_id=?').get(eventId);
    if (!outbox) return [];
    return JSON.parse(outbox.recipients_json)
      .sort((a,b)=>(a.callPriority??1000)-(b.callPriority??1000))
      .slice(0, config.VOICE_MAX_CONTACTS);
  }

  function launchNext(eventId) {
    if (!provider.configured) return;
    const dispatch = db.prepare('SELECT * FROM voice_dispatches WHERE event_id=?').get(eventId);
    if (dispatch?.acknowledged) return;
    if (db.prepare("SELECT 1 FROM voice_attempts WHERE event_id=? AND status IN ('QUEUED','CALLING')").get(eventId)) return;
    const contacts = contactsFor(eventId);
    let chosen, number;
    for (let round=1;round<=config.VOICE_MAX_ATTEMPTS_PER_CONTACT && !chosen;round++) {
      for (const contact of contacts) {
        const exists=db.prepare('SELECT 1 FROM voice_attempts WHERE event_id=? AND contact_id=? AND attempt=?').get(eventId,contact.id,round);
        if (!exists) { chosen=contact;number=round;break; }
      }
    }
    if (!chosen) {
      db.prepare("UPDATE voice_dispatches SET status='EXHAUSTED',updated_at_ms=? WHERE event_id=? AND acknowledged=0").run(now(),eventId);
      return;
    }
    const time=now();
    db.prepare("INSERT OR IGNORE INTO voice_dispatches(event_id,provider,status,created_at_ms,updated_at_ms) VALUES(?,?,'PENDING',?,?)").run(eventId,provider.name,time,time);
    const inserted=db.prepare("INSERT OR IGNORE INTO voice_attempts(event_id,contact_id,contact_name,phone,call_priority,attempt,status,created_at_ms,updated_at_ms) VALUES(?,?,?,?,?,?,'QUEUED',?,?)")
      .run(eventId,chosen.id,chosen.name,chosen.phone,chosen.callPriority??1000,number,time,time);
    if (!inserted.changes) return;
    launch({eventId,contactId:chosen.id,contactName:chosen.name,phone:chosen.phone,callPriority:chosen.callPriority??1000,attempt:number});
  }

  function start(eventId) { launchNext(eventId); }

  function handleStatus(eventId,contactId,number,status) {
    if (!TERMINAL.has(status)) return false;
    const dispatch=db.prepare('SELECT acknowledged FROM voice_dispatches WHERE event_id=?').get(eventId);
    if (!dispatch) return false;
    const changed=db.prepare("UPDATE voice_attempts SET status=?,updated_at_ms=? WHERE event_id=? AND contact_id=? AND attempt=? AND status IN ('QUEUED','CALLING')")
      .run(status,now(),eventId,contactId,number).changes;
    if (changed && !dispatch.acknowledged) launchNext(eventId);
    return !!changed;
  }

  function handleGather(eventId,contactId,number,digits) {
    if (digits !== '1') return false;
    if (!db.prepare('SELECT 1 FROM voice_attempts WHERE event_id=? AND contact_id=? AND attempt=?').get(eventId,contactId,number)) return false;
    const changed=db.prepare("UPDATE voice_dispatches SET status='ACKNOWLEDGED',acknowledged=1,acknowledged_contact_id=?,acknowledged_at_ms=?,updated_at_ms=? WHERE event_id=? AND acknowledged=0")
      .run(contactId,now(),now(),eventId).changes;
    return !!changed;
  }

  function callback(action,eventId,contactId,number,body,headers,path) {
    const publicUrl=config.PUBLIC_CALLBACK_BASE_URL?.replace(/\/$/,'')+path;
    if (!validateTwilioSignature(config.TWILIO_AUTH_TOKEN,publicUrl,body,headers['x-twilio-signature'])) throw new ApiError(403,'INVALID_PROVIDER_SIGNATURE','Invalid Twilio callback signature');
    const row=db.prepare('SELECT 1 FROM voice_attempts WHERE event_id=? AND contact_id=? AND attempt=?').get(eventId,contactId,number);
    if (!row) missing();
    if (action==='status') handleStatus(eventId,contactId,number,body.CallStatus);
    const event=db.prepare('SELECT latitude,longitude,display_name FROM safety_events WHERE event_id=?').get(eventId);
    const base=config.PUBLIC_CALLBACK_BASE_URL.replace(/\/$/,'');
    const twiml=acknowledged=>buildVoiceTwiml({displayName:event?.display_name??'Người dùng FallSafe',smsStatus:transport.aggregateSms(eventId),hasLocation:event?.latitude!=null&&event?.longitude!=null,gatherUrl:`${base}/api/v1/voice/twilio/${encodeURIComponent(eventId)}/${encodeURIComponent(contactId)}/${number}/gather`,acknowledged});
    if (action==='gather') {
      handleGather(eventId,contactId,number,body.Digits);
      return {raw:twiml(body.Digits==='1'),contentType:'application/xml; charset=utf-8'};
    }
    if (action!=='twiml') return {raw:'<?xml version="1.0" encoding="UTF-8"?><Response></Response>',contentType:'application/xml; charset=utf-8'};
    return {raw:twiml(false),contentType:'application/xml; charset=utf-8'};
  }

  return {
    start, handleStatus, handleGather, callback,
    status(eventId) {
      const event=db.prepare('SELECT event_id FROM safety_events WHERE event_id=?').get(eventId);if(!event)missing();
      const dispatch=db.prepare('SELECT * FROM voice_dispatches WHERE event_id=?').get(eventId);
      return {eventId,provider:dispatch?.provider??provider.name,providerConfigured:!!provider.configured,status:dispatch?.status??'PENDING',acknowledged:!!dispatch?.acknowledged,attempts:db.prepare('SELECT * FROM voice_attempts WHERE event_id=? ORDER BY call_priority,attempt').all(eventId).map(attempt)};
    },
  };
}
