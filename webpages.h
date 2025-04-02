const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<title>GIF Upload (=^･ω･^=)</title>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    #currentGif {
      max-width: 280px;
      max-height: 240px;
      border: 2px solid #ddd;
      margin: 10px auto;
      display: block;
    }
    .gif-container {
      text-align: center;
      margin-bottom: 20px;
    }
    .else-container {
      text-align: center;
    }
  </style>
</head>
<body>
  <p>(=^･ω･^=) GIF Upload to ESP32 (=^･ω･^=)</p>
  
  <div class="gif-container">
    <h3>Current GIF Preview</h3>
    <img id="currentGif" src="/file?name=/img.gif&action=download" 
         onerror="this.style.display='none'" alt="No GIF loaded">
  </div>

  <div class="else-container">
    <p>Note: Please make sure either the max height of the GIF is less than 240px and the max width is less than 280px<br><br>
  STEP 1: Delete the previous GIF<br><br>
  STEP 2: Reboot the ESP32<br><br>
  STEP 3: Upload your new GIF (if it's glitchy, reboot it again.)<br><br>
  Playback speed might be slow, just a limitation of the ESP32.
  <p>Maximum GIF size: <strong>1MB</strong>. Larger files will be rejected.</p>
  
  <p>Free Storage: <span id="freespiffs">%FREESPIFFS%</span> | 
     Used Storage: <span id="usedspiffs">%USEDSPIFFS%</span> | 
     Total Storage: <span id="totalspiffs">%TOTALSPIFFS%</span></p>
  
  <p>
    <button onclick="resetWiFiButton()">Reset WiFi Config</button>
    <button onclick="logoutButton()">Logout</button>
    <button onclick="rebootButton()">Reboot</button>
    <button onclick="listFilesButton()">List Files</button>
    <button onclick="showUploadButtonFancy()">Upload File</button>
  </p>
  
  <p id="status"></p>
  <p id="detailsheader"></p>
  <p id="details"></p>
  </div>

<script>
function resetWiFiButton() {
  if (confirm("Are you sure you want to reset WiFi configuration?")) {
    fetch('/resetwifi')
      .then(response => response.text())
      .then(text => alert(text));
  }
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}

function rebootButton() {
  document.getElementById("status").innerHTML = "Rebooting...";
  fetch('/reboot')
    .then(response => window.location.href = "/reboot")
    .catch(error => console.error('Reboot failed:', error));
}

function listFilesButton() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}

function downloadDeleteButton(filename, action) {
  var urltocall = "/file?name=" + filename + "&action=" + action;
  xmlhttp=new XMLHttpRequest();
  if (action == "delete") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
    document.getElementById("status").innerHTML = xmlhttp.responseText;
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    document.getElementById("details").innerHTML = xmlhttp.responseText;
    refreshGifPreview();
  }
  if (action == "download") {
    document.getElementById("status").innerHTML = "";
    window.open(urltocall,"_blank");
  }
}

function showUploadButtonFancy() {
  document.getElementById("detailsheader").innerHTML = "<h3>Upload File<h3>";
  document.getElementById("status").innerHTML = "";
  var uploadform =
  "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
  "<input type=\"file\" name=\"file1\" id=\"file1\" onchange=\"uploadFile()\"><br>" +
  "<progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>" +
  "<h3 id=\"status\"></h3>" +
  "<p id=\"loaded_n_total\"></p>" +
  "</form>";
  document.getElementById("details").innerHTML = uploadform;
}

function _(el) {
  return document.getElementById(el);
}

function uploadFile() {
  var file = _("file1").files[0];
  
  // Client-side size check
  if (file.size > 1048576) {
    _("status").innerHTML = "ERROR: File must be <1MB";
    _("progressBar").value = 0;
    return;
  }

  var formdata = new FormData();
  formdata.append("file1", file);
  
  var ajax = new XMLHttpRequest();
  ajax.upload.addEventListener("progress", progressHandler, false);
  ajax.addEventListener("load", function(e) {
    if (this.status == 413) {
      _("status").innerHTML = "SERVER ERROR: File too large (max 1MB)";
      _("progressBar").value = 0;
    } else {
      completeHandler(e);
    }
  }, false);
  ajax.addEventListener("error", errorHandler, false);
  ajax.addEventListener("abort", abortHandler, false);
  ajax.open("POST", "/");
  ajax.send(formdata);
}

function progressHandler(event) {
  _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = Math.round(percent) + "% uploaded... please wait";
  if (percent >= 100) {
    _("status").innerHTML = "Please wait, writing file to filesystem";
  }
}

function refreshGifPreview() {
  var gif = document.getElementById('currentGif');
  gif.style.display = 'block';
  gif.src = "/file?name=/img.gif&action=download&t=" + new Date().getTime();
}

function completeHandler(event) {
  _("status").innerHTML = "Upload Complete";
  _("progressBar").value = 0;
  
  // Refresh file list
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
  
  // Update GIF preview
  refreshGifPreview();
  
  // Update status
  document.getElementById("status").innerHTML = "File Uploaded - GIF refreshed!";
}

function errorHandler(event) {
  _("status").innerHTML = "Upload Failed";
}

function abortHandler(event) {
  _("status").innerHTML = "Upload Aborted";
}

// Load GIF preview when page loads
window.onload = function() {
  refreshGifPreview();
};
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><a href="/">Log Back In</a></p>
</body>
</html>
)rawliteral";

const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
<h3>
  Rebooting, returning to main page in <span id="countdown">5</span> seconds
</h3>
<script type="text/javascript">
  var seconds = 5;
  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      window.location = "/";
    } else {
      document.getElementById("countdown").innerHTML = seconds;
      window.setTimeout("countdown()", 1000);
    }
  }
  countdown();
</script>
</body>
</html>
)rawliteral";