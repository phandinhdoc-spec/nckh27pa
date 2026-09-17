import {transaction} from '../repositories/db.js';
import {eventRepository} from '../repositories/eventRepository.js';
import {alertRepository} from '../repositories/alertRepository.js';
import {alertOutboxService} from './alertOutboxService.js';
export function startWatchdog({db,config,now}) {
 const events=eventRepository(db),alerts=alertRepository(db),outbox=alertOutboxService(db);
 let healthy=true;
 function scan() {
  let failed=false;
  try {
   for(const {event_id:id} of alerts.due(now())) {
    try {
     transaction(db,()=>{
      // Recheck under the write transaction, including after another process's scan.
      const event=events.get(id),time=now();
      if(event.alertState!=='VERIFYING'||!event.watchdogActive||time<event.watchdogDeadlineMs)return;
      const count=outbox.record({...event,response:'NO_RESPONSE'},time);
      alerts.sos(id,time,count,'NO_RESPONSE','COUNTDOWN_TIMEOUT',event.location);
      alerts.timeout(id);alerts.recordAction(id,'sos',time);
     });
    } catch {failed=true;}
   }
  } catch {failed=true;}
  healthy=!failed;
 }
 scan();
 const timer=setInterval(scan,config.WATCHDOG_SCAN_INTERVAL_MS);
 return {scan,isHealthy:()=>healthy,stop:()=>clearInterval(timer)};
}
