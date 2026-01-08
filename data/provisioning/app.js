document.getElementById('prov').addEventListener('submit', async function(e){
  e.preventDefault();
  const form = e.target;
  const data = new URLSearchParams(new FormData(form));
  const status = document.getElementById('status');
  status.textContent = 'Saving...';

  try{
    // Some captive-portal captive browsers (or mobile browsers) block fetch/post due to
    // DNS/HTTPS redirection quirks. Send an explicit content-type header and timeout.
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 8000);
    
    const res = await fetch('/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: data.toString(),
      signal: controller.signal
    });
    
    clearTimeout(timeout);
    
    if (!res.ok) {
      status.textContent = 'Server error: ' + res.status;
      return;
    }
    
    const txt = await res.text();
    status.innerHTML = txt;
  }catch(err){
    // Fallback: some captive portals block fetch; try a plain form submit which
    // is more likely to be allowed by the browser's captive-portal handling.
    console.warn('Fetch failed, falling back to native submit', err);
    status.textContent = 'Fetch failed, falling back to form submit...';
    // Remove event listener and submit normally
    form.removeEventListener('submit', arguments.callee);
    form.submit();
  }
});
