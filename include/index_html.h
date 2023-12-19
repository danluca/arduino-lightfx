const char index_html[] PROGMEM = R"~~~(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8">
    <title>Luca Light FX</title>
    <link href="pixel.css" rel="stylesheet" type="text/css" />
    <script src="jquery.min.js" type="text/javascript"></script>
    <script src="pixel.js" type="text/javascript"></script>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
</head>

<body>
    <header>
        <div>LucaFx - Lucas Style Light Effects</div>
        <div>Board: <span id="boardName"></span></div>
    </header>
    <main>
        <div id="updateStatus"></div>
        <section id="effects">
            <h1>Effects</h1>
            <div id="fxChangeArea">
                <div id="fxSelectArea">
                <label for="fxlist" id="fxListLabel">Current light effect</label>
                <select id="fxlist" onchange="updateEffect()">
                    <option value="none">Select an effect</option>
                </select>
                </div>
                <div id="fxAutoChangeArea">
                    <input type="checkbox" id="autoFxChange" onchange="updateAuto()"/>
                    <label for="autoFxChange" id="autoFxChangeLabel">Automatic Effect change</label>
                </div>
                <div id="fxBrightChangeArea">
                    <label for="brightList" id="brightListLabel">Strip brightness</label>
                    <select id="brightList" onchange="updateBrightness()">
                        <option value="0">Auto</option>
                        <option value="100">100%</option>
                        <option value="90">90%</option>
                        <option value="80">80%</option>
                        <option value="70">70%</option>
                        <option value="60">60%</option>
                        <option value="50">50%</option>
                        <option value="40">40%</option>
                        <option value="30">30%</option>
                        <option value="20">20%</option>
                        <option value="10">10%</option>
                    </select>
                </div>
            </div>
            <div id="curEffectArea">
                <p id="curEffectLabel">Current effect: </p>
                <p id="curEffect"></p>
                <p id="curEffectId"></p>
            </div>
        </section>
        <section id="time">
            <h1>Time</h1>
            <div id="timeChangeArea">
                <label for="holidayList" id="holidayListLabel">Change current color theme</label>
                <select id="holidayList" onchange="updateHoliday()">

                </select>
            </div>
            <div id="curHolidayArea">
                <p><span id="curHolidayLabel">Current color theme: </span><span id="curHolidayValue"></span></p>
            </div>
        </section>
        <section id="settings">
            <h1>Settings</h1>
            <div id="settingsChange">
                TBD
            </div>
        </section>
        <section id="status">
            <h1 onclick="getStatus()">Status</h1>
            <dl id="statusArea">
                <div>
                <dt>Board</dt>
                <dd>Mbed OS: <span id="mbedVersion"></span></dd>
                <dd>Temperature: <span id="boardTemp"></span> <br/><span id="rangeTemp" class="indent2"></span></dd>
                <dd>Vcc: <span id="boardVcc"></span> <br/><span id="rangeVcc" class="indent2"></span></dd>
                <dd>Audio Threshold: <span id="audioThreshold"></span></dd>
                <dd>Up Time: <span id="upTime"></span> </dd>
                <dd>Status code: <span id="overallStatus"></span></dd>
                </div>
                <div>
                <dt>WiFi</dt>
                <dd>IP Address: <span id="wfIpAddress"></span></dd>
                <dd>Signal: <span id="wfSignal"></span></dd>
                <dd>WiFi FW: <span id="wfVersion"></span></dd>
                </div>
                <div>
                <dt>Effects</dt>
                <dd>Total: <span id="fxCount"></span></dd>
                <dd>Current: <span id="fxCurEffect"></span></dd>
                <dd>Past Effects: <span id="pastEffects"></span></dd>
                <dd>Color Theme: <span id="fxCurHoliday"></span></dd>
                <dd>Strip Brightness: <span id="fxBrightness"></span></dd>
                <dd>Audio Effect Changes: <span id="totalAudioBumps"></span></dd>
                <dd>Audio Level Histogram: <span id="audioLevelHistogram"></span></dd>
                </div>
                <div>
                <dt>Time</dt>
                <dd>NTP sync: <span id="timeNtp"></span></dd>
                <dd>Current time: <span id="timeCurrent"></span></dd>
                <dd>Holiday: <span id="timeHoliday"></span></dd>
                </div>
            </dl>
        </section>

    </main>
    <footer>
        <div>Version <span id="buildVersion"></span> built at <span id="buildTime"></span> on <span id="buildBranch"></span></div>
        <div>Copyright &copy; 2023 Dan Luca. All rights reserved.</div>
    </footer>
</body>

</html>
)~~~";
