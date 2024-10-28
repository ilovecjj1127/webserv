#!/usr/bin/env php
<?php

header("Content-Type: text/html");

$current_time = date("Y-m-d H:i:s");

$body = "<html><head>" .
        "<style>
            .time-display {
                font-size: 1.5em;
                color: #FF5733;
                font-weight: bold;
            }
        </style>" .
        "<script type='text/javascript'>
            function updateTime() {
                var currentTime = new Date();
                var formattedTime = currentTime.getFullYear() + '-' + 
                                    ('0' + (currentTime.getMonth() + 1)).slice(-2) + '-' + 
                                    ('0' + currentTime.getDate()).slice(-2) + ' ' + 
                                    ('0' + currentTime.getHours()).slice(-2) + ':' + 
                                    ('0' + currentTime.getMinutes()).slice(-2) + ':' + 
                                    ('0' + currentTime.getSeconds()).slice(-2);
                document.getElementById('time').innerHTML = formattedTime;
            }
            setInterval(updateTime, 1000); // Update the time every second
        </script>" .
        "</head><body>\n<h1>PHP CGI: Current Time</h1>" .
        "<p><span class='time-display'>The current server time is:  <span id='time'>" . htmlspecialchars($current_time) . "</span></span></p>" .
        "</body></html>";

$response = "Status: 200 OK\r\n" .
            "Content-Type: text/html\r\n" .
            "Content-Length: " . strlen($body) . "\r\n\r\n" .
            $body;

echo $response;
?>





