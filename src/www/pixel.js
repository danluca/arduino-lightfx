
// Only one sequence can be selected
let config = {};
//canvasjs column chart options
let histOptions = {
    theme: "light2",
    backgroundColor: "#F4F4FD",
    title: {
        text: "Audio Levels",
        fontSize: 18
    },
    data: [
        {
            type: "column",
            dataPoints: [
            ]
        }
    ]
};

$(() => {
    $('#curYear').html(`-${new Date().getFullYear()}`);

    $.getJSON( "config.json")
    .done(function( data ) {
        config = data;
        let curFxId = data.curEffect;
        let fxlst = $('#fxlist');
        $.each( data.fx, function( i, fxi ) {
            let fx_id = fxi.registryIndex;
            //let fx_name = fxi.name;
            let fx_description = overflowString(fxi.description, 60);

            // Add to list
            fxlst.append(`<option class="opt-select" value="${fx_id}">${fx_description}</option>`);
        });
        fxlst.val(curFxId);
        fxlst.attr("currentFxIndex", curFxId);
        $('#curEffect').html(`${data.curEffectName} - ${data.fx[data.curEffect].description}`);
        $('#curEffectId').html(`Index: ${data.curEffect}`);
        $('#autoFxChange').prop("checked", data.auto);
        $('#sleepEnabled').prop("checked", data.fx.sleepEnabled);
        $('#curHolidayValue').html(data.holiday);
        $.each(data.holidayList, function(i, hld) {
            if (hld == "None") {
                $('#holidayList').append(`<option class="opt-select" value="${hld}">Automatic</option>`);
            } else {
                $('#holidayList').append(`<option class="opt-select" value="${hld}">${hld}</option>`);
            }
        });
        $('#holidayList').val(data.holiday);
        $('#boardName').html(data.boardName);
        $('#deviceName').html(data.boardName);
        $('#boardUid').html(data.boardUid);
        $('#buildVersion').html(data.fwVersion);
        $('#buildBranch').html(data.fwBranch);
        $('#buildTime').html(data.buildTime);
        $('#macAddress').html(data.MAC);
        $('#cleanBoot').html(`${data.cleanBoot}`);
        let wdr = data.watchdogRebootsCount === 0 ? `${data.watchdogRebootsCount}` : `${data.watchdogRebootsCount}<br/> [last @ ${data.lastWatchdogReboot}]`;
        $('#wdReboots').html(wdr);
    });
    getStatus();
    setInterval(getStatus, 2*60*1000);  //every 2 minutes update status

});

function getStatus() {
    $.getJSON("status.json")
        .done(function (data) {
            $('#status h1').removeClass('red');
            $('#boardTemp').html(`${data.boardTemp.toFixed(1)} °C (${(data.boardTemp*9/5+32).toFixed(1)} °F)`);
            $('#rangeTemp').html(`[${data.boardMinTemp.toFixed(1)} - ${data.boardMaxTemp.toFixed(1)}] °C (chip ${data.chipTemp.toFixed(1)} °C)`);
            $('#boardVcc').html(`${data.vcc.toFixed(2)} V`);
            $('#rangeVcc').html(`[${data.minVcc.toFixed(2)} - ${data.maxVcc.toFixed(2)}] V`);
            $('#mbedVersion').html(`${data.mbedVersion}`);
            $('#audioThreshold').html(`${data.fx.audioThreshold}`);
            $('#upTime').html(`${data.upTime}`);
            $('#overallStatus').html(`0x${data.overallStatus.toString(16).toUpperCase()}`);
            $('#wfIpAddress').html(`${data.wifi.IP}`);
            $('#wfSignal').html(`${data.wifi.bars} bars (${data.wifi.rssi} dB)`);
            if (data.wifi.curVersion !== data.wifi.latestVersion) {
                $('#wfVersion').html(`WiFi NINA v${data.wifi.curVersion} [could upgrade to ${data.wifi.latestVersion}]`);
            } else {
                $('#wfVersion').html(`WiFi NINA v${data.wifi.curVersion} (latest)`);
            }
            $('#fxCount').html(`${data.fx.count} effects`);
            if (data.fx.asleep) {
                $('#fxCurEffect').html(`${data.fx.name} - asleep [${data.fx.index}]`);
            } else {
                $('#fxCurEffect').html(`${data.fx.name} [${data.fx.index}]`);
            }
            $('#pastEffects').html(`${data.fx.pastEffects.reverse().join(', ')}`);
            $('#totalAudioBumps').html(`${data.fx.totalAudioBumps}`);
            let lblHistogram = data.fx.audioHist.map((elem, ix)=>
                `${ix===(data.fx.audioHist.length-1)?'>':''}${data.fx.audioThreshold+ix*500}`);
                // `${(data.fx.audioThreshold+ix*500)/1000} - ${(data.fx.audioThreshold+(ix+1)*500)/1000}k${ix===(data.fx.audioHist.length-1)?'+':''}`);
                // `${data.fx.audioThreshold+ix*500} - ${data.fx.audioThreshold+(ix+1)*500}${ix===(data.fx.audioHist.length-1)?'+':''} : ${elem}`);
            //$('#audioLevelHistogram').html(`${lblHistogram.join('<br/>')}`);
            histOptions.data[0].dataPoints = data.fx.audioHist.map((elem, ix) => {
                return { label: lblHistogram[ix], y: elem };
            });
            $("#audioHistogram").CanvasJSChart(histOptions);
            $('#timeNtp').html(`${data.time.ntpSync == 2}`);
            $('#timeCurrent').html(`${data.time.date} ${data.time.time} ${data.time.dst?"CDT":"CST"}`);
            $('#timeHoliday').html(`${data.time.holiday}`);
            $('#lastDrift').html(`${data.time.lastDrift} ms`);
            $('#avgDrift').html(`${data.time.averageDrift} ms/hr`);
            $('#totalDrift').html(`${data.time.totalDrift} ms (${data.time.syncSize} sync points @ 17 hrs)`);

            let strAlarms = "";
            data.time.alarms.sort((a, b) => a.timeLong - b.timeLong);
            data.time.alarms.forEach(al =>  strAlarms += `<li>${al.timeFmt} (${al.type})</li>`);
            let strAlarmsEnabled = data.fx.sleepEnabled ? "" : "<i>(alarms disabled)</i>"
            $('#schAlarms').html(`${strAlarmsEnabled}${strAlarms.length > 0 ? "<br/><ul>"+strAlarms+"</ul>" : ": None"}`);

            //update the current effect tiles as well
            $('#curEffectId').html(`index: ${data.fx.index}`);
            let desc = config?.fx?.find(x=> x.registryIndex === data.fx.index)?.description ?? "N/A";
            $('#curEffect').html(`${desc}`);
            $('#autoFxChange').prop("checked", data.fx.auto);
            $('#sleepEnabled').prop("checked", data.fx.sleepEnabled);
            let fxlst = $('#fxlist');
            fxlst.val(data.fx.index);
            fxlst.attr("currentFxIndex", data.fx.index);
            $('#curHolidayValue').html(data.fx.holiday);
            let hdlst = $('#holidayList');
            hdlst.val(data.fx.holiday);
            hdlst.attr("currentColorTheme", data.fx.holiday);
            //approximately undo the dimming rules in FastLED library where dimming raw is equivalent with x*x/256
            let brPerc = Math.round(Math.sqrt(data.fx.brightness * 256)*100/256);
            $('#fxBrightness').html(`${brPerc}% (${data.fx.brightness}${data.fx.brightnessLocked?' fixed':' auto'})`)
            let brList = $('#brightList');
            brList.val(brPerc);
            brList.attr("currentBrightness", brPerc);

        })
        .fail(function (req, textStatus, error){
            console.log(`status.json call failed ${textStatus} - ${error}`);
            $('#status h1').addClass('red');
        });
}

function updateEffect() {
    let fxlst = $('#fxlist');
    let selectedFx = fxlst.val();
    if (selectedFx == "none") {
        return;
    }
    if (selectedFx != fxlst.attr("currentFxIndex")) {
        let request = {};
        request["effect"] = parseInt(selectedFx);
        $.ajax({
            type: "PUT",
            url: "/fx",
            contentType: "application/json",
            dataType: "json",
            data: JSON.stringify(request),
            success: function (response) {
                fxlst.attr("currentFxIndex", selectedFx);
                $('#curEffect').html(`${config.fx[selectedFx].name} - ${config.fx[selectedFx].description}`)
                $('#updateStatus').html("Effect update successful").removeClass().addClass("status-ok");
                scheduleClearStatus();
            },
            error: function (request, status, error) {
                $('#updateStatus').html(`Effect update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
                fxlst.val(fxlst.attr("currentFxIndex"));
                scheduleClearStatus();
            }
        });
    }
}

function updateAuto() {
    let selectedAuto = $('#autoFxChange').prop("checked");
    let request = {};
    request["auto"] = selectedAuto;
    $.ajax({
        type: "PUT",
        url: "/fx",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(request),
        success: function (response) {
            $('#updateStatus').html(`Automatic effects loop ${selectedAuto ? 'enabled' : 'disabled'} successfully`).removeClass().addClass("status-ok");
            scheduleClearStatus();
        },
        error: function (request, status, error) {
            $('#updateStatus').html(`Automatic effects loop update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
            $('#autoFxChange').prop("checked", !selectedAuto);
            scheduleClearStatus();
        }
    });
}

function updateSleep() {
    let selectedSleep = $('#sleepEnabled').prop("checked");
    let request = {};
    request["sleepEnabled"] = selectedSleep;
    $.ajax({
        type: "PUT",
        url: "/fx",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(request),
        success: function (response) {
            $('#updateStatus').html(`Sleep schedule ${selectedSleep ? 'enabled' : 'disabled'} successfully`).removeClass().addClass("status-ok");
            scheduleClearStatus();
        },
        error: function (request, status, error) {
            $('#updateStatus').html(`Sleep schedule update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
            $('#sleepEnabled').prop("checked", !selectedSleep);
            scheduleClearStatus();
        }
    });
}

function updateHoliday() {
    let hldlst = $('#holidayList');
    let selHld = hldlst.val();
    let request = {};
    request["holiday"] = selHld;
    $.ajax({
        type: "PUT",
        url: "/fx",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(request),
        success: function (response) {
            $('#updateStatus').html("Color theme update successful").removeClass().addClass("status-ok");
            hldlst.attr("currentColorTheme", selHld);
            $('#curHolidayValue').html(selHld);
            scheduleClearStatus();
        },
        error: function (request, status, error) {
            $('#updateStatus').html(`Color theme update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
            hldlst.val(hldlst.attr("currentColorTheme"));
            scheduleClearStatus();
        }
    });
}

function updateBrightness() {
    let brlst = $('#brightList');
    let selBr = brlst.val();
    let request = {};
    request["brightness"] = Math.round(Math.pow(selBr*256/100, 2)/256);
    //cap it at 0xFF (1 byte)
    if (request["brightness"] > 255)
        request["brightness"] = 255;
    $.ajax({
        type: "PUT",
        url: "/fx",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(request),
        success: function (response) {
            $('#updateStatus').html("Strip brightness update successful").removeClass().addClass("status-ok");
            brlst.attr("currentBrightness", selBr);
            let brPerc = Math.round(Math.sqrt(response.updates.brightness * 256)*100/256);
            $('#fxBrightness').html(`${brPerc}% (${response.updates.brightness}${response.updates.brightnessLocked?' fixed':' auto'})`);
            scheduleClearStatus();
        },
        error: function (request, status, error) {
            $('#updateStatus').html(`Strip brightness update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
            brlst.val(brlst.attr("currentBrightness"));
            scheduleClearStatus();
        }
    });
}

function scheduleClearStatus() {
    setTimeout(function () {
        $('#updateStatus').removeClass().html("");
    }, 5000);
}

function overflowString(str, ovrThreshold) {
    if (str.length > ovrThreshold) {
        return str.substring(0, ovrThreshold-3) + "...";
    }
    return str;
}
