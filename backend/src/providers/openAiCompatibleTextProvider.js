async function boundedText(response, limit) {
  const declared=Number(response.headers.get('content-length'));
  if(Number.isFinite(declared)&&declared>limit){await response.body?.cancel();throw new Error('provider-response-too-large');}
  if(!response.body) return '';
  const reader=response.body.getReader();
  const decoder=new TextDecoder('utf-8',{fatal:true});
  let size=0,text='';
  try {
    while(true){
      const {done,value}=await reader.read();
      if(done)break;
      size+=value.byteLength;
      if(size>limit){await reader.cancel();throw new Error('provider-response-too-large');}
      text+=decoder.decode(value,{stream:true});
    }
    return text+decoder.decode();
  } finally { reader.releaseLock(); }
}

function outputText(payload) {
  if(typeof payload?.output_text==='string'&&payload.output_text.trim())return payload.output_text;
  const values=[];
  for(const item of Array.isArray(payload?.output)?payload.output:[]) {
    if(item?.type!=='message'||!Array.isArray(item.content))continue;
    for(const content of item.content)if(content?.type==='output_text'&&typeof content.text==='string')values.push(content.text);
  }
  return values.join('').trim()||null;
}

export function openAiCompatibleTextProvider(config,{fetchImpl=globalThis.fetch}={}) {
  const configured=config.AI_PROVIDER==='OPENAI_COMPATIBLE'&&[config.AI_BASE_URL,config.AI_MODEL,config.AI_API_KEY]
    .every(value=>typeof value==='string'&&value.trim());
  if(!configured)return {name:'DISABLED',configured:false,async requestText(){return {status:'NOT_CONFIGURED'};}};
  const endpoint=`${config.AI_BASE_URL.replace(/\/$/,'')}/responses`;
  return {
    name:'OPENAI_COMPATIBLE',configured:true,
    async requestText(text,{signal}={}) {
      const response=await fetchImpl(endpoint,{
        method:'POST',signal,
        headers:{authorization:`Bearer ${config.AI_API_KEY}`,'content-type':'application/json'},
        body:JSON.stringify({model:config.AI_MODEL,input:text,max_output_tokens:config.AI_MAX_OUTPUT_TOKENS,store:false}),
      });
      if(!response.ok){await response.body?.cancel();return {status:'UNAVAILABLE'};}
      const raw=await boundedText(response,config.AI_PROVIDER_RESPONSE_LIMIT_BYTES);
      let payload;try{payload=JSON.parse(raw);}catch{throw new Error('invalid-provider-response');}
      const value=outputText(payload);
      if(!value)throw new Error('invalid-provider-response');
      return {status:'COMPLETED',text:value};
    },
  };
}
