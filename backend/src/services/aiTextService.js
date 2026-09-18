import { ApiError, invalid } from './errors.js';
import { string } from './validation.js';

export function aiTextService({config,aiProvider}) {
  return {
    async request(body) {
      if(!body||Object.keys(body).length!==1||!Object.hasOwn(body,'text'))invalid('Invalid AI text request');
      const text=string(body.text,'AI text',config.AI_MAX_INPUT_CHARS).trim();
      if(!aiProvider?.configured)throw new ApiError(503,'NOT_CONFIGURED','Optional AI is not configured');
      const controller=new AbortController();
      let timer;
      const deadline=new Promise((_,reject)=>{
        timer=setTimeout(()=>{controller.abort();reject(new Error('ai-timeout'));},config.AI_REQUEST_TIMEOUT_MS);
      });
      let result;
      try {
        result=await Promise.race([aiProvider.requestText(text,{signal:controller.signal}),deadline]);
      } catch {
        throw new ApiError(503,'SERVICE_UNAVAILABLE','Optional AI is temporarily unavailable');
      } finally { clearTimeout(timer); }
      if(result?.status==='NOT_CONFIGURED')throw new ApiError(503,'NOT_CONFIGURED','Optional AI is not configured');
      if(result?.status!=='COMPLETED'||typeof result.text!=='string'||!result.text.trim())throw new ApiError(503,'SERVICE_UNAVAILABLE','Optional AI is temporarily unavailable');
      return {status:'COMPLETED',text:result.text.trim().slice(0,config.AI_MAX_OUTPUT_CHARS)};
    },
  };
}
