/* Runs only a bounded sample-reply regex in a disposable worker. Never executes a command/script. */
(function(root){
  'use strict';
  function parseReply(pattern,reply){
    if(typeof pattern!=='string'||pattern.length>500||typeof reply!=='string'||reply.length>4000)return {error:'Sample pattern or reply exceeds the preview limit.'};
    try{const match=new RegExp(pattern,'m').exec(reply);if(!match)return {matched:false,fields:{}};return {matched:true,fields:match.groups?{...match.groups}:{}};}
    catch(error){return {error:'Invalid regular expression: '+error.message};}
  }
  if(typeof module!=='undefined'&&module.exports)module.exports={parseReply};
  else root.onmessage=e=>root.postMessage(parseReply(e.data.pattern,e.data.reply));
})(typeof self!=='undefined'?self:globalThis);
