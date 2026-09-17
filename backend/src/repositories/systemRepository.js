export function activeAlertsCount(db) {
  return db.prepare("SELECT count(*) AS n FROM safety_events WHERE alert_state != 'MONITORING'").get().n;
}
