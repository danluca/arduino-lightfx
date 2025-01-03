
/*
 * Copyright (c) 2025 by Dan Luca. All rights reserved.
 *
 */

// Only one sequence can be selected
let config = {};
//canvasjs column chart options
let histOptions = {
    theme: "light2",
    backgroundColor: "#F4F4FD",
    title: {
        text: "Tasks Runtime",
        fontSize: 18
    },
    axisY: {
        minimum: 0
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

    getTasks();
    setInterval(getTasks, 60*1000);  //every 1 minutes update status

});

function getTasks() {
    $.getJSON("tasks.json")
        .done(function (data) {
            $('#tasks h1').removeClass('red');
            $('#curTime').html(`Time: ${data.date} ${data.time}`);
            $('#boardName').html(data.boardName);
            $('#deviceName').html(data.boardName);
            $('#boardUid').html(data.boardUid);
            $('#buildVersion').html(data.fwVersion);
            $('#buildBranch').html(data.fwBranch);
            $('#buildTime').html(data.buildTime);
            let tableBody = $('#taskTableBody');
            tableBody.empty();
            data.tasks.items.sort((a,b)=> a.taskNumber - b.taskNumber);
            $.each(data.tasks.items, function (i, task) {
                let priority = task.curPriority === task.basePriority ? `${task.curPriority}` : `${task.curPriority} / ${task.basePriority}`;
                tableBody.append(
                    `<tr>
                        <td>${task.taskNumber}</td>
                        <td>${task.name}</td>
                        <td>${task.state}</td>
                        <td>${priority}</td>
                        <td>#${task.coreAffinity.toString(16).toUpperCase()}</td>
                        <td>${task.stackHighWaterMark}</td>
                        <td>${task.runTime.toLocaleString()}</td>
                        <td>${task.runTimePct.toFixed(2)}</td>
                    </tr>`)

            });
            let memList = $('#memoryList');
            memList.empty();
            memList.append(`<li>Free Stack: <span>${data.heap.freeStack.toLocaleString()} bytes [stack pointer: #${data.heap.stackPointer.toString(16)}]</span></li>`);
            memList.append(`<li>Total Heap: <span>${data.heap.totalHeap.toLocaleString()} bytes</span></li>`);
            memList.append(`<li>Free Heap: <span>${data.heap.freeHeap.toLocaleString()} bytes</span></li>`);
            memList.append(`<li>Logging buffer space: <span>${data.heap.logMinBufferSpace.toLocaleString()} bytes</span></li>`);
            let miscArea = $('#miscDetailArea');
            miscArea.empty();
            miscArea.append(`<p>System reported runtime: <span>${data.tasks.sysTotalRunTime.toLocaleString()}</span></p>`);
            miscArea.append(`<p>Tasks total runtime: <span>${data.tasks.tasksTotalRunTime.toLocaleString()}</span></p>`);
            miscArea.append(`<p>Total runtime: <span>${(data.millis).toLocaleString()} ms</span></p>`);
            miscArea.append(`<p>Cycles 32bit: <span>${data.cycles32.toLocaleString()}</span></p>`);
            miscArea.append(`<p>Cycles 64bit: <span>${data.cycles64.toLocaleString()}</span></p>`);


            let lblHistogram = data.tasks.items.map((elem, ix)=> `${elem.name}`);
            histOptions.data[0].dataPoints = data.tasks.items.map((elem, ix) => {
                return { label: lblHistogram[ix], y: elem.runTimePct, toolTipContent: `Runtime: ${elem.runTime}` };
            });
            $("#taskStats").CanvasJSChart(histOptions);
        })
        .fail(function (req, textStatus, error){
            console.log(`tasks.json call failed ${textStatus} - ${error}`);
            $('#tasks h1').addClass('red');
        });
}


function overflowString(str, ovrThreshold) {
    if (str.length > ovrThreshold) {
        return str.substring(0, ovrThreshold-3) + "...";
    }
    return str;
}
