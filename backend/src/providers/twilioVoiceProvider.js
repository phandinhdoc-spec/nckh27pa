import { createHmac, timingSafeEqual } from 'node:crypto';
import { request as httpsRequest } from 'node:https';

function xml(value) {
  return String(value).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;').replaceAll("'",'&apos;');
}

export function validateTwilioSignature(authToken, url, params, supplied) {
  if (!authToken || !url || !supplied) return false;
  const payload=url+Object.keys(params).sort().map(key=>key+params[key]).join('');
  const expected=createHmac('sha1',authToken).update(payload).digest('base64');
  const left=Buffer.from(expected);const right=Buffer.from(supplied);
  return left.length===right.length && timingSafeEqual(left,right);
}

export function buildVoiceTwiml({ displayName='Người dùng FallSafe', smsStatus, hasLocation, gatherUrl, automatedIntro=true, acknowledged=false }) {
  const location = ['SENT','DELIVERED'].includes(smsStatus) && hasLocation
    ? 'Đã gửi liên kết vị trí qua tin nhắn.'
    : hasLocation ? 'Vị trí đang chờ gửi qua tin nhắn.' : 'Vị trí hiện chưa xác định.';
  const message=`Cảnh báo: ${displayName} có thể đã bị ngã và cần trợ giúp. ${location} Nhấn phím 1 để xác nhận đã nhận cảnh báo.`;
  const intro=automatedIntro?'<Say language="vi-VN">Đây là cuộc gọi tự động từ FallSafe.</Say>':'';
  const repetitions=acknowledged?3:4;
  const repeated=Array.from({length:repetitions},()=>`<Say language="vi-VN">${xml(message)}</Say><Pause length="3"/>`).join('');
  const content=acknowledged?repeated:`<Gather action="${xml(gatherUrl)}" method="POST" numDigits="1" timeout="15">${repeated}</Gather>`;
  return `<?xml version="1.0" encoding="UTF-8"?><Response>${intro}${content}</Response>`;
}

export function twilioVoiceProvider(config) {
  const configured=config.VOICE_PROVIDER==='TWILIO' && !!(config.TWILIO_ACCOUNT_SID&&config.TWILIO_AUTH_TOKEN&&config.TWILIO_FROM_NUMBER&&config.PUBLIC_CALLBACK_BASE_URL);
  return {
    name: configured?'TWILIO':'DISABLED', configured,
    createCall(attempt) {
      if (!configured) throw new Error('provider-unconfigured');
      const base=config.PUBLIC_CALLBACK_BASE_URL.replace(/\/$/,'');
      const callback=`${base}/api/v1/voice/twilio/${encodeURIComponent(attempt.eventId)}/${encodeURIComponent(attempt.contactId)}/${attempt.attempt}`;
      const formParams=new URLSearchParams({To:attempt.phone,From:config.TWILIO_FROM_NUMBER,Url:`${callback}/twiml`,StatusCallback:`${callback}/status`,StatusCallbackMethod:'POST'});
      for(const value of ['initiated','ringing','answered','completed'])formParams.append('StatusCallbackEvent',value);
      const form=formParams.toString();
      return new Promise((resolve,reject)=>{
        const req=httpsRequest({hostname:'api.twilio.com',port:443,path:`/2010-04-01/Accounts/${encodeURIComponent(config.TWILIO_ACCOUNT_SID)}/Calls.json`,method:'POST',headers:{authorization:`Basic ${Buffer.from(`${config.TWILIO_ACCOUNT_SID}:${config.TWILIO_AUTH_TOKEN}`).toString('base64')}`,'content-type':'application/x-www-form-urlencoded','content-length':Buffer.byteLength(form)}},res=>{
          let body='';res.setEncoding('utf8');res.on('data',chunk=>body+=chunk);res.on('end',()=>{
            if(res.statusCode<200||res.statusCode>=300)return reject(new Error(`Twilio HTTP ${res.statusCode}`));
            try{const parsed=JSON.parse(body);if(!parsed.sid)throw new Error('Missing call SID');resolve({providerCallId:parsed.sid});}catch(error){reject(error);}
          });
        });
        req.setTimeout(10000,()=>req.destroy(new Error('Twilio timeout')));req.on('error',reject);req.end(form);
      });
    },
  };
}
