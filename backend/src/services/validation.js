import { invalid } from './errors.js';
export function string(value, name, max = 128) { if (typeof value !== 'string' || !value.trim() || value.length > max) invalid(`Invalid ${name}`); return value; }
export function number(value, name, min = -Infinity, max = Infinity, integer = false) { if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max || (integer && !Number.isSafeInteger(value))) invalid(`Invalid ${name}`); return value; }
export const integer = (v, n, max = Number.MAX_SAFE_INTEGER) => number(v,n,0,max,true);
export function boolean(value,name) { if (typeof value !== 'boolean') invalid(`Invalid ${name}`); return value; }
export function enumeration(value,name,values) { if (!values.includes(value)) invalid(`Invalid ${name}`); return value; }
export function nullable(value,name,check = number) { if (value === null) return null; return check(value,name); }
export function location(value) {
 if (value === undefined || value === null) return null;
 if (typeof value !== 'object' || Array.isArray(value)) invalid('Invalid location');
 number(value.latitude,'latitude',-90,90); number(value.longitude,'longitude',-180,180); number(value.accuracyM,'accuracyM',0); integer(value.timestampMs,'location timestampMs'); string(value.locationMessage,'locationMessage',2000);
 return {latitude:value.latitude,longitude:value.longitude,accuracyM:value.accuracyM,timestampMs:value.timestampMs,locationMessage:value.locationMessage};
}
export const source = v => enumeration(v,'sensorSource',['PHONE','ESP32']);
