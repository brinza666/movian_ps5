# SPDX-License-Identifier: GPL-3.0-or-later
# Requires the optional host fixture packages pyftpdlib and impacket.
import ftplib,io,os,socket,subprocess,sys,tempfile,threading,json,zipfile,hashlib
from pathlib import Path
from impacket.smbserver import SimpleSMBServer
from impacket.ntlm import compute_lmhash,compute_nthash
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

def port():
 with socket.socket() as s:
  s.bind(('127.0.0.1',0));return s.getsockname()[1]

exe=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='movian-network-') as temp:
 root=Path(temp);media=root/'media';media.mkdir();(media/'Nested').mkdir()
 payload=bytes(range(256))*1000
 (media/'a sample.mkv').write_bytes(payload)
 (media/'Nested'/'nested.mkv').write_bytes(payload)
 def bencode(value):
  if isinstance(value,int):return b'i'+str(value).encode()+b'e'
  if isinstance(value,dict):return b'd'+b''.join(bencode(k)+bencode(v) for k,v in sorted(value.items()))+b'e'
  return str(len(value)).encode()+b':'+value
 piece=65536
 info={b'name':b'a sample.mkv',b'length':len(payload),b'piece length':piece,
       b'pieces':b''.join(hashlib.sha1(payload[n:n+piece]).digest() for n in range(0,len(payload),piece))}
 (media/'Nested'/'sample.torrent').write_bytes(bencode({b'info':info}))
 with zipfile.ZipFile(media/'Nested'/'package.zip','w',zipfile.ZIP_DEFLATED) as z:
  z.writestr('plugin.json',json.dumps({'id':'network-fixture','title':'Network fixture','version':'1','apiversion':2,'file':'main.js'}))
  z.writestr('main.js','var fixture = true;')
 (media/'Nested'/'invalid.zip').write_bytes(b'not a ZIP')
 with (media/'Nested'/'oversized.zip').open('wb') as out:out.truncate(8*1024*1024+1)
 (media/'readme.txt').write_text('hidden by media filter')
 (media/('z'*150+'.mkv')).write_bytes(payload)
 smbport,ftpport,serverport=port(),port(),port()
 smb=SimpleSMBServer(listenAddress='127.0.0.1',listenPort=smbport)
 smb.addShare('media',str(media));smb.setSMB2Support(True)
 smb.addCredential('fixture',0,compute_lmhash('fixture-pass'),compute_nthash('fixture-pass'))
 smb.setLogFile(str(root/'smb.log'))
 threading.Thread(target=smb.start,daemon=True).start()
 a=DummyAuthorizer();a.add_user('fixture','fixture-pass',str(media),perm='elr')
 FTPHandler.authorizer=a
 ftp=FTPServer(('127.0.0.1',ftpport),FTPHandler)
 threading.Thread(target=ftp.serve_forever,kwargs={'timeout':.2},daemon=True).start()
 env=os.environ.copy();env.update(MOVIAN_TEST_SMB_URL=f'smb2://127.0.0.1:{smbport}/media',
  MOVIAN_TEST_FTP_URL=f'ftp://127.0.0.1:{ftpport}/',MOVIAN_TEST_FTP_PORT=str(serverport))
 try:
  subprocess.run([str(exe),'clients'],cwd=root,env=env,check=True,timeout=70)
  (root/'data/movian').mkdir(parents=True);(root/'usb').mkdir()
  (root/'download0'/'clip.bin').write_bytes(payload)
  (root/'outside.txt').write_text('must stay private')
  (root/'download0'/'escape').symlink_to('../outside.txt')
  (root/'download0'/'alias').symlink_to('network-private.bin')
  p=subprocess.Popen([str(exe),'server'],cwd=root,env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
  try:
   assert p.stdout.readline().strip()=='READY'
   f=ftplib.FTP();f.connect('127.0.0.1',serverport,5)
   try:f.login('fixture','wrong');raise AssertionError('wrong password accepted')
   except ftplib.error_perm as e:assert str(e).startswith('530')
   f.login('fixture','fixture-pass')
   assert set(f.nlst())=={'Title data','Movian library','USB 0'}
   f.cwd('Title data')
   assert not any('network-private' in n for n in f.nlst())
   for name in ('network-private.bin','alias','escape'):
    try:f.retrbinary('RETR '+name,lambda b:None);raise AssertionError(name+' readable')
    except ftplib.error_perm as e:assert str(e).startswith('550')
   b=io.BytesIO();f.retrbinary('RETR clip.bin',b.write,rest=1024)
   assert b.getvalue()==payload[1024:]
   upload=b'disposable network upload'
   f.storbinary('STOR upload.bin',io.BytesIO(upload))
   assert (root/'download0'/'upload.bin').read_bytes()==upload
   f.cwd('..');f.cwd('Movian library')
   f.storbinary('STOR library.bin',io.BytesIO(upload))
   assert (root/'data/movian'/'library.bin').read_bytes()==upload
   f.quit()
   env['MOVIAN_TEST_FTP_URL']=f'ftp://127.0.0.1:{serverport}/'
   subprocess.run([str(exe),'list-server'],cwd=root,env=env,check=True,timeout=30)
   print('FTP server authentication, roots, REST, RETR, STOR and path protection passed')
  finally:
   p.communicate('stop\n',timeout=8)
   assert p.returncode==0
 finally:
  ftp.close_all();smb.stop()
