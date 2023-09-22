const char pixel_js[] PROGMEM = R"~~~(

// Only one sequence can be selected
let config = {};

$(() => {

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
        $('#curHolidayValue').html(data.holiday);
        $.each(data.holidayList, function(i, hld) {
            if (hld == "None") {
                $('#holidayList').append(`<option class="opt-select" value="${hld}">Automatic</option>`);
            } else {
                $('#holidayList').append(`<option class="opt-select" value="${hld}">${hld}</option>`);
            }
        });
        $('#holidayList').val(data.holiday);

    });
    getStatus();
    setInterval(getStatus, 2*60*1000);  //every 2 minutes update status

});

function getStatus() {
    $.getJSON("status.json")
        .done(function (data) {
            $('#status h1').removeClass('red');
            $('#boardTemp').html(`${data.boardTemp} 'C (${data.boardTemp*9/5+32} 'F)`);
            $('#wfIpAddress').html(`${data.wifi.IP}`);
            $('#wfSignal').html(`${data.wifi.bars} bars (${data.wifi.rssi} dB)`);
            $('#fxCount').html(`${data.fx.count} effects`);
            $('#fxCurEffect').html(`${data.fx.name} [${data.fx.index}]`);
            $('#fxCurHoliday').html(`${data.fx.holiday}`);
            $('#timeNtp').html(`${data.time.ntpSync == 2}`);
            $('#timeCurrent').html(`${data.time.date} ${data.time.time} CST`);
            $('#timeHoliday').html(`${data.time.holiday}`);

            //update the current effect tiles as well
            $('#curEffectId').html(`Index: ${data.fx.index}`);
            let desc = config.fx.find(x=> x.registryIndex === data.fx.index)?.description ?? "N/A";
            $('#curEffect').html(`${data.fx.name} - ${desc}`);
            let fxlst = $('#fxlist');
            fxlst.val(data.fx.index);
            fxlst.attr("currentFxIndex", data.fx.index);
            $('#curHolidayValue').html(data.fx.holiday);
            let hdlst = $('#holidayList');
            hdlst.val(data.fx.holiday);
            hdlst.attr("currentColorTheme", data.fx.holiday);
            $('#fxBrightness').html(`${(data.fx.brightness/256*100.0).toFixed(2)} % (${data.fx.brightness}${data.fx.brightnessLocked?' fixed':' auto'})`)
            let brList = $('#brightList');
            brList.val(data.fx.brightness);
            brList.attr("currentBrightness", data.fx.brightness);
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
    request["brightness"] = selBr;
    $.ajax({
        type: "PUT",
        url: "/fx",
        contentType: "application/json",
        dataType: "json",
        data: JSON.stringify(request),
        success: function (response) {
            $('#updateStatus').html("Strip brightness update successful").removeClass().addClass("status-ok");
            brlst.attr("currentBrightness", selBr);
            $('#fxBrightness').html(`${(data.fx.brightness/256*100.0).toFixed(2)} % (${data.fx.brightness}${data.fx.brightnessLocked?' fixed':' auto'})`);
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

)~~~";
