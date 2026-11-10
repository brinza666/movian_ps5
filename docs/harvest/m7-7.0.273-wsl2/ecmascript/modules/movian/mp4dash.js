exports.mp4_to_hls = function(s, flags)
{
try {

const FAP_PFX = 'fmp4://';
const base64_prefix = "data:application/x-mpegURL;base64,";

var FLAG_LOG = !(flags & 1);
var FLAG_VFS = flags & 2;
var FLAG_CACHE = flags & 4;

if(!flags) {FLAG_LOG = 0; FLAG_VFS = 1; FLAG_CACHE = 1;}

var time_fps = 1000;

var fmp4_file_cache = [];

function get_hls(url, is_video)
{
	if(url && url.indexOf("http")!=0) return null;
	var ua = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36';
	var ac = 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8';
	var se = 'Unable to probe file (index error)';
	var bd = null;
	var to_read = 0;
	for(var rr=2;rr<10;rr+=2)
	{
		to_read = ((rr+0)*4096);
		data = require('movian/http').request(url, {
				headers:{
							'User-Agent': ua,
							'Accept': ac,
							'Range': 'bytes=0-'+(to_read-1),
							'Connection': 'keep-alive',
						},
				caching: false,
				compression: false,
				nofail: true,
				verifySSL: false,
			});
		
		bd = data.bytes;
		
		sidx_off = strpos(bd, 0, 'sidx');
		if(sidx_off) break;
	}
	if(!sidx_off) {console.log(se); return null;}
	sidx_off -= 4;

	sidx_siz = rb32(bd, sidx_off);
	data_off = sidx_off - 1 + sidx_siz;
	if(data_off>10000000) {console.log(se); return null;}
	if(data_off>to_read)
	{
		data = require('movian/http').request(url, {
				headers:{
							'User-Agent': ua,
							'Accept': ac,
							'Range': 'bytes=0-'+(data_off-1),
							'Connection': 'keep-alive',
						},
				caching: false,
				compression: false,
				nofail: true,
				verifySSL: false,
			});
		
		bd = data.bytes;
	}

	time_bas = rb32(bd, sidx_off + 16);
	time_div = 1; if(time_bas>120) { time_div = 1000; if(parseInt(time_bas/1024)==time_bas/1024) time_div = 1024; else if(parseInt(time_bas/512)==time_bas/512) time_div = 512; }
	tfhd_off = strpos(bd, sidx_off, 'tfhd');
	if(tfhd_off>sidx_off)
	{
		tfhd_flg = rb16(bd, tfhd_off + 6) & 0xFF;	
		tfhd_pos = 8;
		
		if(tfhd_flg & 0x01) tfhd_pos+=8;
		if(tfhd_flg & 0x02) tfhd_pos+=8;
		if(tfhd_flg & 0x08)
		{
			time_div = rb32(bd, tfhd_off + tfhd_pos);
			if(FLAG_LOG) console.log((is_video?'Video':'Audio')+' timescale: '+time_bas+'/'+time_div);
		}
	}
	
	if(is_video)
	{
		time_fps = round(((1000*time_bas)/time_div)/1000, 3);
		if(FLAG_LOG) console.log('Video framerate: '+time_fps+' fps');
	}
	
	sidx_ent = rb16(bd, sidx_off + 30) & 0x7FFF;

	idx = [];
	idx_base = sidx_off + 32;
	total_dur = 0;
	off = 1;
	for(var e=0; e<sidx_ent; e++)
	{
		siz = rb32(bd, idx_base);
		dur = rb32(bd, idx_base+4);
		dur = (time_div*dur)/(time_div*time_bas);
		idx.push({size: siz, dur: dur, data: (data_off+off)});
		total_dur += dur;
		off += siz;
		idx_base+=12;	
	}

	var seg = [];
	seg.push(get_hls_header(url, 0, sidx_off, round(total_dur/sidx_ent, 0)));
	
	for(var i=0;i<idx.length;i++)
		seg.push(['#EXTINF:',(round(idx[i].dur, 6)),',\n#EXT-X-BYTERANGE:',(idx[i].size),(!i?'@'+(idx[i].data):''),'\n',url,'\n'].join(''));

	seg.push('#EXT-X-ENDLIST\n');

	return seg.join("");		
}

function get_master(s)
{
	var a = '';
	var m = '';
	var pfx = s.prefix?s.prefix+'_':(get_hash('sha1', JSON.stringify(s)).substr(0, 16)+'_');

	if(s.audio)
	for(var i=0;i<s.audio.length;i++)
	{
		ae = s.audio[i];
		if(!ae.url) continue;
		lang = ae.lang?ae.lang:'eng';
		name = ae.name?ae.name:'Audio';
		def = !i?'YES':'NO';
		af = pfx+'audio'+i+'.m3u8';
		if(!FLAG_VFS)
		{
			m = get_hls(ae.url, 0);
			if(!m) continue;
			write_m3u8(af, m);
		}
		else
			af = FAP_PFX + Duktape.enc('base64', JSON.stringify( {url: ae.url, n: af} )) + '.m3u8';
		
		a += '#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="M7A",LANGUAGE="'+lang+'",NAME="'+name+'",DEFAULT='+def+',AUTOSELECT='+def+',URI="'+af+'"\n';
	}
	
	var v = '';
	if(s.video)
	for(var i=0;i<s.video.length;i++)
	{
		ve = s.video[i];
		if(!ve.url) continue;
		vf = pfx+'video'+i+'.m3u8';
		bw = ve.bandwidth?ve.bandwidth:3000000;
		w  = ve.width?ve.width:1920;
		h  = ve.height?ve.height:1080;
		c  = ve.codec?ve.codec:'avc1.640028';
		vr = ve.hdr?ve.hdr:'SDR';
		if(!FLAG_VFS)
		{
			m = get_hls(ve.url, 1);
			if(!m) continue;
			write_m3u8(vf, m);
			fr = (time_fps<1000?time_fps:(ve.fps?ve.fps:'25.000'));
		}
		else
		{
			vf = FAP_PFX + Duktape.enc('base64', JSON.stringify( {url: ve.url, n: vf} )) + '.m3u8';
			fr = ve.fps?ve.fps:(time_fps<1000?time_fps:'25.000');
		}
		
		time_fps = 1000;
		v += '#EXT-X-STREAM-INF:BANDWIDTH='+bw+',AVERAGE-BANDWIDTH='+bw+',CODECS="'+c+'",RESOLUTION='+w+'x'+h+',AUDIO="M7A",VIDEO-RANGE='+vr+',FRAME-RATE='+fr+'\n'+vf+'\n';
	}
	
	master = null;
	if(a || v) 
	{
		master = '#EXTM3U\n#EXT-X-VERSION:6\n'+(a?a:'')+(v?v:'');
		var mf = pfx+'master.m3u8';
		if(!FLAG_VFS)
		{
			return ((Core.storagePath.indexOf('persistent://')<0 || Core.storagePath[0]=='/')?'file://':'')+write_m3u8(mf, master);
		}
		else
		{
			fmp4_file_cache[mf] = Duktape.enc('base64', master);
			return FAP_PFX + Duktape.enc('base64', JSON.stringify( {url: base64_prefix + fmp4_file_cache[mf], n: mf} )) + '.m3u8';
		}
	}
	
	return null;
}

var fap = require('native/faprovider');

var fmp4_cache = [];

fap.register('fmp4', {

  redirect: function(handle, url) {
	try {
	var d = JSON.parse(Duktape.dec('base64', url.replace(".m3u8", "")));
	var fn = Core.storagePath + '/m3u8/' + d.n;
	if(d.url && fmp4_file_cache[d.n]) fn = base64_prefix + fmp4_file_cache[d.n];
	else if(d.url && d.url.indexOf("data:")==0) fn = d.url;
	var now = get_ts();
	if(FLAG_CACHE && fmp4_cache[url] && fmp4_cache[url].ts && fmp4_cache[url].ts < now + 3600)
	{
		fap.redirectRespond(handle, true, fn);
		return;
	}
	else
	{
		var m = "";
		if(d.url == "data" && fmp4_file_cache[d.n])
			m = Duktape.dec('base64', fmp4_file_cache[d.n]);
		else
		if(d.url.indexOf("data:")==0)
			m = Duktape.dec('base64', d.url.substr(34));
		else
			m = get_hls(d.url, (d.n.indexOf("video")>=0?1:0));

		if(m)
		{
			fmp4_cache[url] = {ts: now};
			if(FLAG_CACHE && FLAG_VFS)
			{
				url = Duktape.enc('base64', JSON.stringify( {url: "data", n: d.n} )) + '.m3u8';
				fmp4_file_cache[d.n] = Duktape.enc('base64', m);
				fmp4_cache[url] = {ts: now};
				fap.redirectRespond(handle, true, base64_prefix + fmp4_file_cache[d.n]);
				return;
			}
			fap.redirectRespond(handle, true, write_m3u8(d.n, m));
			return;
		}
		else
		if(fmp4_cache[url] && fmp4_cache[url].ts && fmp4_cache[url].ts < now + 7200)
		{
			fap.redirectRespond(handle, true, fn);
			return;
		}
	}
	} catch(err){;}
    fap.redirectRespond(handle, false, 'Request failed');
  }
});
function get_ts(){var date = new Date();return parseInt(date.getTime()/1000);}
var get_hash = function(algo, str) { var crypto = require('native/crypto'); var hash = crypto.hashCreate(algo); crypto.hashUpdate(hash, str); var digest = crypto.hashFinalize(hash); return Duktape.enc('hex', digest);}
function rb16(b, p) { return ((b[p + 0])*256 +(b[p + 1])); }
function rb32(b, p) { return ((b[p + 0])*16777216 + (b[p + 1])*65536 + (b[p + 2])*256 + (b[p + 3])); }
function round(number, digits) { var multiple = Math.pow(256, digits); var rndedNum = Math.round(number * multiple) / multiple; return rndedNum; }
function strpos(b, s, n) { var ret = null; var fp = null; var np = 0; for(var i=s; i<(b.length-n.length); i++) { if(b[i]==n.charCodeAt(np)) { if(!fp) fp=i; np++; } else { np = 0; fp = null; } if(np==n.length) break; } return fp; }
function get_hls_header(map, s, e, d) {	return '#EXTM3U\n#EXT-X-TARGETDURATION:'+d+'\n#EXT-X-VERSION:6\n#EXT-X-MEDIA-SEQUENCE:0\n#EXT-X-PLAYLIST-TYPE:VOD\n#EXT-X-INDEPENDENT-SEGMENTS\n#EXT-X-MAP:URI="'+map+'",BYTERANGE="'+e+'@'+s+'"\n'; }
function write_m3u8(n, d) { var path = Core.storagePath + '/m3u8/';	var fss = require('fs'); var fs = require('native/fs');	fs.mkdirs(path); fss.writeFileSync(path+n, d); return path+n; }
return get_master(s); } catch (err) {return null;}
}