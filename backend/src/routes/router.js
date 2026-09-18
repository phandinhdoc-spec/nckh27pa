import { missing, invalid } from '../services/errors.js';
function decode(value) {try{return decodeURIComponent(value);}catch{invalid('Invalid URL encoding');}}
export function route(method, url, actions, body, headers) {
  if (method === 'GET' && url.pathname === '/api/v1/system/status') return actions.system();
  if (method === 'POST' && url.pathname === '/api/v1/ai/text') return actions.aiText(body);
  const device=url.pathname.match(/^\/api\/v1\/devices\/([^/]+)\/status$/);
  if(device && ['GET','POST'].includes(method)) return actions.device(method,decode(device[1]),body,headers);
  if(method==='POST' && url.pathname==='/api/v1/sensors/ingest') return actions.sensor(method,null,body);
  if(method==='GET' && url.pathname==='/api/v1/sensors/latest') return actions.sensor(method,url.searchParams.get('deviceId'));
  if(url.pathname==='/api/v1/events' && ['GET','POST'].includes(method)) return actions.event(method,null,body,url.searchParams);
  const event=url.pathname.match(/^\/api\/v1\/events\/([^/]+)$/);
  if(event && method==='GET') return actions.event(method,decode(event[1]));
  const contact=url.pathname.match(/^\/api\/v1\/users\/([^/]+)\/contacts(?:\/([^/]+)(?:\/(primary|toggle-sos))?)?$/);
  if(contact) {
    const [,user,id,action]=contact;
    if((!id && ['GET','POST'].includes(method)) || (id && !action && ['PUT','DELETE'].includes(method)) || (action && method==='POST')) return actions.contact(method,decode(user),id===undefined?undefined:decode(id),action,body);
  }
  if(method==='GET' && url.pathname==='/api/v1/alerts/active') return actions.alert('active',body,url.searchParams);
  const dispatch=url.pathname.match(/^\/api\/v1\/alerts\/([^/]+)\/dispatch$/);
  if(method==='GET' && dispatch) return actions.voiceStatus(decode(dispatch[1]));
  const transport=url.pathname.match(/^\/api\/v1\/alerts\/([^/]+)\/transport-status$/);
  if(method==='POST' && transport) return actions.transportStatus(decode(transport[1]),body,headers);
  const voice=url.pathname.match(/^\/api\/v1\/voice\/twilio\/([^/]+)\/([^/]+)\/([0-9]+)\/(twiml|gather|status)$/);
  if(method==='POST' && voice) return actions.voiceCallback(voice[4],decode(voice[1]),decode(voice[2]),Number(voice[3]),body,headers,url.pathname);
  const alert=url.pathname.match(/^\/api\/v1\/alerts\/(cancel|sos|acknowledge|resolve)$/);
  if(method==='POST' && alert) return actions.alert(alert[1],body);
  missing();
}
