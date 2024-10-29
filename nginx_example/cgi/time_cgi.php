#!/usr/bin/env php
<?php

function get_time() {
    $raw_post_data = file_get_contents("php://input");
    var_dump($_POST);
    var_dump($raw_post_data);
    var_dump($_SERVER);

    $timezone = isset($_POST['timezone']) ? htmlspecialchars($_POST['timezone']) : 'UTC';
    $dateTime = new DateTime("now", new DateTimeZone($timezone));
    $body = "<html><head>" .
            "<style>
                .time-display {
                    font-size: 1.5em;
                    color: #FF5733;
                    font-weight: bold;
                }
            </style>" .
            "</head><body>\n<h1>PHP CGI: Current Time</h1>" .
            "<p><span class='time-display'>The current server time in " . htmlspecialchars($timezone) .
            " is:  <span id='time'>" . htmlspecialchars($dateTime->format("Y-m-d H:i:s")) . "</span></span></p>" .
            "</body></html>";
    return $body;
}

function get_options() {
    $timezones = timezone_identifiers_list();
    $endpoint = isset($_SERVER['PATH_INFO']) ? $_SERVER['PATH_INFO'] : '';
    
    $body = "<html><body>" .
            "<h1>PHP CGI: Current Time</h1>" .
            "<form method='POST' action='" . htmlspecialchars($endpoint) . "'>" .
            "<label for='timezone'>Choose a timezone:  </label>" .
            "<select name='timezone' id='timezone'>";

    foreach ($timezones as $timezone) {
        $body .= "<option value='" . htmlspecialchars($timezone) . "'>" . htmlspecialchars($timezone) . "</option>";
    }
            
    $body .= "</select>" .
             "<input type='submit' value='Show Time'>" .
             "</form></body></html>";
    return $body;
}

function unknown_method($method) {
    $body = "<html><body><h1>PHP CGI \"Current Time\"</h1>" .
            "<h2>405 Method $method is not allowed</h2></body></html>";
    $response = "Status: 405 Method Not Allowed\r\nContent-Type: text/html\r\n" .
                "Content-Length: " . strlen($body) . "\r\n\r\n" .
                $body;
    echo $response;
    exit;
}
        

function handle_request() {
    $method = $_SERVER['REQUEST_METHOD'];
    $body = "";

    if ($method === 'GET') {
        $body = get_options();
    } elseif ($method === 'POST') {
        $body = get_time();
    } else {
        return unknown_method($method);
    }

    $response = "Status: 200 OK\r\n" .
                "Content-Type: text/html\r\n" .
                "Content-Length: " . strlen($body) . "\r\n\r\n" .
                $body;
    echo $response;
}

handle_request();
?>





