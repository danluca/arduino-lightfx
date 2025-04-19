#pragma once
inline constexpr auto pixel_css PROGMEM = R"~~~(
/* Set padding and margins to 0 then change later
Helps with cross browser support */
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
#links {
    float: right;
    font-size: 14pt;
    padding-right: 2em;
    text-align: right;
}

#effects, #time, #status, #settings {
    border: #afafaf solid 1px;
    border-radius: 10px;
    padding: 2em 1em;
    margin-top: 2em;
    background-color: #F4F4FD;
    float: left;
    width: 100%;
    box-sizing: border-box;
}

#effects #fxChangeArea {
    float: left;
}

#effects #fxChangeArea #fxListLabel {
    color: #7f7f7f;
    display: block;
    padding: 5px 0;
}

#fxChangeArea #fxAutoChangeArea, #fxSleepEnabledArea, #fxBroadcastEnabledArea, #fxChangeArea #fxBrightChangeArea, #curHolidayLabel, #holidayListLabel {
    color: #7f7f7f;
    padding-left: 1em;
    padding-right: 2em;
    float: left;
}
#curHolidayLabel {
    padding-left: 6em;
}
#holidayListLabel {
    color: #7f7fBf;
    padding-left: 0;
}
#curHolidayValue {
    float: left;
}
#effects .separator {
    float: left;
    width: 100%;
}

#brightListLabel {
}

#effects #fxChangeArea #fxlist {
    text-overflow: ellipsis;
}

#effects #curEffectArea, #effects #timeChangeArea, #effects #curHolidayArea {
    float: left;
}

#curEffectLabel {
    color: #7f7fBf;
    display: block;
    padding: 5px 0;
}

#status div {
    padding-left: 1em;
    padding-bottom: 10px;
}
#status div p span {
    color: #7f7f7f;
}
#schAlarms ul {
    padding-left: 2em;
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

#audioLevelHistogram {
    padding-left: 2em;
    display: inline-block;
}

.red {
    color: red;
}

main {
    display: flow-root;
}

dt {
    font-weight: bold;
    font-size: 16px;
    margin-top: 1em;
    margin-bottom: 0.5em;
}

dd {
    color: #7f7f7f;
    margin-left: 1em;
}

dl div {
    float: left;
    width: 250pt;
}

dd span {
    color: black;
}
.indent2 {
    padding-left: 2em;
}
#audioHistogram {
    width: 400px;
    height: 300px;
    padding-left: 0 !important;
}
.canvasjs-chart-credit {
    display: none;
}
)~~~";
