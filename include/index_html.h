const char index_html[] PROGMEM = R"~~~(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8">
    <title>Luca Light FX</title>
    <link href="pixel.css" rel="stylesheet" type="text/css" />
    <script src="jquery.min.js" type="text/javascript"></script>
<!--    <script src="jquery-ui.min.js"></script>-->
    <script src="pixel.js" type="text/javascript"></script>
</head>

<body>
    <header>LucaFx - Lucas Style Light Effects</header>
    <main>
        <div id="updateStatus"></div>
        <section id="effects">
            <h1>Effects</h1>
            <div id="fxChangeArea">
                <div id="fxSelectArea">
                <label for="fxlist" id="fxListLabel">Change current light effect</label>
                <select id="fxlist" onchange="updateEffect()">
                    <option value="none">Select an effect</option>
                </select>
                </div>
                <div id="fxAutoChangeArea">
                    <input type="checkbox" id="autoFxChange" onchange="updateAuto()"/>
                    <label for="autoFxChange" id="autoFxChangeLabel">Automatic Effect change</label>
                </div>
            </div>
            <div id="curEffectArea">
                <label for="curEffect" id="curEffectLabel">Current effect: </label>
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
            <h1>Status</h1>
            <div id="statusArea">

            </div>
        </section>

    </main>
    <footer>Copyright &copy; 2023 Dan Luca. All rights reserved.</footer>
</body>

</html>
)~~~";
