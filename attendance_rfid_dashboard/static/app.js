const $ = (id) => document.getElementById(id);
const today = new Date();
const isoToday = new Date(today.getTime() - today.getTimezoneOffset()*60000).toISOString().slice(0,10);
const fmtDate = (v) => v ? new Date(v+'T00:00:00').toLocaleDateString('id-ID',{day:'2-digit',month:'short',year:'numeric'}) : '—';
const fmtTime = (v) => v ? new Date(v).toLocaleTimeString('id-ID',{hour:'2-digit',minute:'2-digit',hour12:false,timeZone:'Asia/Jakarta'}) : '—';
const initials = (name='') => name.split(' ').slice(0,2).map(x=>x[0]).join('').toUpperCase();
const esc = (s='') => String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const statusBadge = (s) => `<span class="status ${s}">${s==='late'?'Late':s==='on_time'?'On time':'Unknown'}</span>`;
async function api(url){const r=await fetch(url);if(!r.ok){const t=await r.text();throw new Error(t)}return r.json()}
function toast(msg){const el=$('toast');el.textContent=msg;el.classList.add('show');setTimeout(()=>el.classList.remove('show'),2800)}
function studentCell(name){return `<div class="student-cell"><span class="student-avatar">${esc(initials(name))}</span>${esc(name)}</div>`}
function setView(view){
 document.querySelectorAll('.view').forEach(x=>x.classList.toggle('active',x.id===`view-${view}`));
 document.querySelectorAll('.nav-item').forEach(x=>x.classList.toggle('active',x.dataset.view===view));
 $('page-title').textContent=({overview:'Dashboard Overview',attendance:'Attendance Records',students:'Student Directory',unknown:'Unknown RFID Cards'})[view];
 if(view==='attendance')loadAttendance();if(view==='students')loadStudents();if(view==='unknown')loadUnknown();
}
document.querySelectorAll('.nav-item').forEach(b=>b.addEventListener('click',()=>setView(b.dataset.view)));
document.querySelectorAll('[data-goto]').forEach(b=>b.addEventListener('click',()=>setView(b.dataset.goto)));
async function loadOverview(){
 const selected=$('overview-date').value;
 try{
  const d=await api(selected?`/api/dashboard?day=${selected}`:'/api/dashboard'),s=d.summary;
  if(!$('overview-date').value)$('overview-date').value=s.date;
  $('total-students').textContent=s.total_students;$('present-count').textContent=s.present;$('late-count').textContent=s.late;$('absent-count').textContent=s.absent;
  $('ontime-caption').textContent=`${s.on_time} on-time attendance`;
  $('rate-value').textContent=`${s.attendance_rate}%`;$('rate-donut').style.background=`conic-gradient(#16a477 ${s.attendance_rate*3.6}deg,#e9edf4 0deg)`;
  $('rate-present').textContent=s.present;$('rate-absent').textContent=s.absent;
  $('recent-body').innerHTML=d.recent.length?d.recent.map(r=>`<tr><td>${studentCell(r.name)}</td><td>${esc(r.student_no)}</td><td class="uid">${esc(r.uid||'—')}</td><td>${fmtTime(r.tapped_at)}</td><td>${esc(r.device)}</td><td>${statusBadge(r.status)}</td></tr>`).join(''):'<tr><td colspan="6" class="empty-state">No attendance records for this date.</td></tr>';
 }catch(e){toast('Dashboard error: '+e.message)}
}
async function loadTrends(){
 try{
  const data=await api(`/api/trends?days=${$('trend-days').value}`);
  if(!data.length){$('trend-chart').innerHTML='<div class="empty-state">No trend data available.</div>';return}
  const max=Math.max(...data.map(x=>Number(x.present)),1);
  $('trend-chart').innerHTML=data.map(x=>`<div class="bar-group" title="${fmtDate(x.day)}: ${x.present} present, ${x.late} late"><div class="bar" style="height:${Math.max(2,x.present/max*100)}%"></div><div class="bar late" style="height:${Math.max(2,x.late/max*100)}%"></div><span class="bar-label">${new Date(x.day).getDate()}</span></div>`).join('');
 }catch(e){$('trend-chart').innerHTML='<div class="empty-state">Unable to load trend data.</div>'}
}
async function loadAttendance(){
 const params=new URLSearchParams({start:$('start-date').value||'',end:$('end-date').value||'',status:$('status-filter').value,search:$('attendance-search').value});
 try{const rows=await api('/api/attendance?'+params);$('attendance-count').textContent=`${rows.length} records found`;
 $('attendance-body').innerHTML=rows.length?rows.map(r=>`<tr><td>${studentCell(r.name)}</td><td>${esc(r.student_no)}</td><td class="uid">${esc(r.uid||'—')}</td><td>${fmtDate(r.attendance_date)}</td><td>${fmtTime(r.tapped_at)}</td><td>${esc(r.device)}</td><td>${statusBadge(r.status)}</td></tr>`).join(''):'<tr><td colspan="7" class="empty-state">No records found.</td></tr>';
 }catch(e){toast('Attendance error: '+e.message)}
}
async function loadStudents(){
 try{const rows=await api('/api/students?search='+encodeURIComponent($('student-search').value));$('student-count').textContent=`${rows.length} students`;
 $('students-body').innerHTML=rows.map(r=>`<tr><td>${studentCell(r.name)}</td><td>${esc(r.student_no)}</td><td class="uid">${esc(r.uid||'Not assigned')}</td><td>${r.attendance_count}</td><td>${r.late_count}</td><td>${r.uid?'<span class="status on_time">Assigned</span>':'<span class="status unknown">No card</span>'}</td></tr>`).join('')||'<tr><td colspan="6" class="empty-state">No students found.</td></tr>';
 }catch(e){toast('Students error: '+e.message)}
}
async function loadUnknown(){
 try{const rows=await api('/api/unknown');$('unknown-count').textContent=`${rows.length} unknown scans`;
 $('unknown-body').innerHTML=rows.map(r=>`<tr><td class="uid">${esc(r.uid)}</td><td>${esc(r.device)}</td><td>${fmtDate(String(r.tapped_at).slice(0,10))} · ${fmtTime(r.tapped_at)}</td><td>${statusBadge('unknown')}</td></tr>`).join('')||'<tr><td colspan="4" class="empty-state">No unknown cards found.</td></tr>';
 }catch(e){toast('Unknown cards error: '+e.message)}
}
async function checkHealth(){
 try{await api('/api/health');$('system-status').textContent='Database connected';$('db-label').textContent='PostgreSQL is responding'}
 catch(e){$('system-status').textContent='Connection issue';$('db-label').textContent='Check DATABASE_URL'}
}
$('today-label').textContent=today.toLocaleDateString('id-ID',{weekday:'short',day:'2-digit',month:'short',year:'numeric'});
$('overview-date').addEventListener('change',loadOverview);$('trend-days').addEventListener('change',loadTrends);
$('apply-filter').addEventListener('click',loadAttendance);
$('student-search').addEventListener('input',()=>{clearTimeout(window.studentTimer);window.studentTimer=setTimeout(loadStudents,250)});
$('export-btn').addEventListener('click',()=>{const p=new URLSearchParams({start:$('start-date').value||'',end:$('end-date').value||''});window.location.href='/api/export.csv?'+p});
$('start-date').value=new Date(Date.now()-30*86400000).toISOString().slice(0,10);$('end-date').value=isoToday;
checkHealth();loadOverview();loadTrends();setInterval(()=>{if($('view-overview').classList.contains('active'))loadOverview();checkHealth()},15000);
