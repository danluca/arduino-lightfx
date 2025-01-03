#pragma once
inline constexpr auto stats_css PROGMEM = R"~~~(
/*
 * Copyright (c) 2025 by Dan Luca. All rights reserved.
 *
 */

/* Set padding and margins to 0 then change later; Helps with cross browser support */
* {
    padding:2px;
    margin:0;
}

header {
    background-color: #6666ff;
    background: linear-gradient(60deg, rgba(131,58,180,1) 0%, rgba(102,102,255,1) 50%, rgba(253,72,29,1) 100%);
    width: 100%;
    height: 120px;
    font-family: Helvetica, sans-serif;
    color: white;
    font-weight: bold;
    font-style: italic;
    font-size: 24pt;
    text-align: center;
    margin-bottom: 1em;
    text-shadow: 3px 3px darkblue;
}

header div:nth-child(2) {
    text-align: right;
    padding-right: 2em;
}

footer {
    color: #AAAAAA;
    text-align: center;
    font-size: 12pt;
    margin-top: 2em;
    width: 95%;
}

h1 {
    padding-bottom: 1em;
}
#links {
    float: left;
    font-size: 14pt;
    padding-left: 2em;
    text-align: left;
}
#curTime {
    float: right;
    text-align: right;
    color: #092845;
    font-family: monospace;
    font-size: 14pt;
    padding-right: 2em;
}

#tasks, #stats {
    border: #afafaf solid 1px;
    border-radius: 10px;
    padding: 2em 1em;
    margin-top: 2em;
    background-color: #F4F4FD;
    float: left;
    width: 100%;
    box-sizing: border-box;
}

#tasks #taskDetailArea {
    padding-bottom: 2em;
}

#taskTable {
    width: 100%;
    border-collapse: collapse;
    border-spacing: 0;
    border: 1px solid #ddd;
}
#taskTable th {
    background-color: #f2f2f2;
    border: 1px solid #ddd;
}
#taskTable td {
    border: 1px solid #ddd;
    color: #0066CC;
}
#taskTable tr:nth-child(even) {
    background-color: #f2f2f2;
}
#taskTable tr:hover {
    background-color: #ddd;
}
#taskTable th, #taskTable td {
    font-size: 14pt;
    text-align: center;
}
#memoryDetailArea {
    font-size: 14pt;
    padding-left: 2em;
}
#memoryDetailArea span {
    color: #0066CC;
}
#miscDetailArea {
    font-size: 14pt;
    padding-left: 1em;
}
#miscDetailArea span {
    color: #0066CC;
}
#stats div {
    padding-left: 1em;
    padding-bottom: 10px;
}
.status-ok {
    background-color: #BBFFBB;
    padding: 1em 0;
    color: darkgreen;
    font-size: 16pt;
    text-align: center;
}

.status-error {
    background-color: #FFBBBB;
    padding: 1em 0;
    color: red;
    font-weight: bold;
    font-size: 18pt;
    text-align: center;
}

#statsChartArea {
    padding-left: 2em;
}
#statsChartArea #taskStats {
    width: 100%;
}

.red {
    color: red;
}

main {
    display: flow-root;
}

.indent2 {
    padding-left: 2em;
}
#taskStats {
    width: 400px;
    height: 300px;
    padding-left: 0 !important;
}
.canvasjs-chart-credit {
    display: none;
}
)~~~";
