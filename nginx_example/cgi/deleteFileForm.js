document.querySelectorAll('.deleteFileForm').forEach(form => {
	form.addEventListener('submit', function(event) {
		event.preventDefault();
		var filename = this.getAttribute('data-filename');
		fetch('/cgi/file_exchange_cgi.py', {
			method: 'DELETE',
			headers: {
				'Content-Type': 'application/x-www-form-urlencoded',
			},
			body: new URLSearchParams({
				'filename': filename
			})
		}).then(response => response.text())
		.then(html => {
			document.body.innerHTML = html;
		})
		.catch(error => console.error('Error:', error));
	});
});
