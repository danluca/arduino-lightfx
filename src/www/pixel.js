
// Only one sequence can be selected
let config = {};

$(() => {

    $.getJSON( "config.json")
    .done(function( data ) {
        config = data;
        let fx_id = "";
        let fx_name = "";
        let fx_description = "";
        let curFxId = data.curEffect;
        $.each( data.fx, function( i, fxi ) {
            fx_id = fxi.registryIndex;
            fx_name = fxi.name;
            fx_description = fxi.description;

            // Add to list
            $('#fxlist').append(`<option class="opt-select" value="${fx_id}">${fx_description}</option>`);
        });
        $('#fxlist').val(curFxId);
        $('#fxlist').attr("currentFxIndex", curFxId);
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
            let stdiv = $('#statusArea');
            stdiv.append(`<div id="boardStatus"><h4>Board</h4><p><span>Temperature:</span> ${data.boardTemp} °C (${data.boardTemp*9/5+32} °F)</p></div>`);
            stdiv.append(`<div id="wifiStatus"><h4>WiFi</h4><p><span>IP Address:</span> ${data.wifi.IP}</p><p><span>Signal:</span> ${data.wifi.bars} bars</p></div>`);
            stdiv.append(`<div id="fxStatus"><h4>Effects</h4><p><span>Total:</span> ${data.fx.count} effects</p><p><span>Current Effect:</span> ${data.fx.name} [${data.fx.index}]</p>
                <p><span>Colors:</span> ${data.fx.holiday}</p></div>`);
            stdiv.append(`<div id="timeStatus"><h4>Time</h4><p><span>NTP synched:</span> ${data.time.ntpSync == 2}</p><p><span>Current Time:</span> ${data.time.date} ${data.time.time} CST</p>
                <p><span>Holiday:</span> ${data.time.holiday}</p></div>`);
            //update the current effect tile as well
            $('#curEffectId').html(`Index: ${data.fx.index}`);
            let desc = config.fx.find(x=> x.registryIndex === data.fx.index).description
            $('#curEffect').html(`${data.fx.name} - ${desc}`);
        });
}

function updateEffect() {
    let selectedFx = $('#fxlist').val();
    if (selectedFx == "none") {
        return;
    }
    if (selectedFx != $('#fxlist').attr("currentFxIndex")) {
        let request = {};
        request["effect"] = parseInt(selectedFx);
        $.ajax({
            type: "PUT",
            url: "/fx",
            contentType: "application/json",
            dataType: "json",
            data: JSON.stringify(request),
            success: function (response) {
                $('#fxlist').attr("currentFxIndex", selectedFx);
                $('#updateStatus').html("Effect update successful").removeClass().addClass("status-ok");
                scheduleClearStatus();
            },
            error: function (request, status, error) {
                $('#updateStatus').html(`Effect update has failed: ${status} - ${error}`).removeClass().addClass("status-error");
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
            $('#updateStatus').html(`Automatic effects loop ${selectedAuto?'enabled':'disabled'} successfully`).removeClass().addClass("status-ok");
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

}

function scheduleClearStatus() {
    setTimeout(function () {
        $('#updateStatus').removeClass().html("");
    }, 5000);
}


