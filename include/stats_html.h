#pragma once
inline constexpr auto stats_html PROGMEM = R"~~~(
<!--
  ~ Copyright (c) 2025 by Dan Luca. All rights reserved.
  ~
  -->

<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="utf-8">
    <title>Light FX Runtime Statistics</title>
    <link href="stats.css" rel="stylesheet" type="text/css" />
    <script src="https://code.jquery.com/jquery-3.7.1.min.js" type="text/javascript"></script>
    <script src="https://cdn.canvasjs.com/jquery.canvasjs.min.js" type="text/javascript"></script>
    <script src="stats.js" type="text/javascript"></script>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
</head>

<body>
<header>
    <div>LucaFx - Runtime Statistics</div>
    <div>Board: <span id="boardName"></span></div>
</header>
<main>
    <div id="links"><a href="index.html">Dashboard</a></div>
    <div id="curTime"></div>
    <section id="tasks">
        <h1>Tasks</h1>
        <div id="taskDetailArea">
            <table id="taskTable">
                <thead>
                    <tr>
                        <th>ID</th>
                        <th>Name</th>
                        <th>State</th>
                        <th>Priority</th>
                        <th>Core</th>
                        <th>Free Stack</th>
                        <th>Runtime</th>
                        <th>Runtime [%]</th>
                    </tr>
                </thead>
                <tbody id="taskTableBody">
                </tbody>
            </table>

        </div>
        <h1>Memory</h1>
        <div id="memoryDetailArea">
            <ul id="memoryList">
            </ul>
        </div><br/>
        <h1>Miscellaneous</h1>
        <div id="miscDetailArea">
        </div>
    </section>
    <section id="stats">
        <h1>Stats</h1>
        <div id="statsChartArea">
            <div id="taskStats"></div>
        </div>
    </section>

</main>
<footer>
    <div>Version <span id="buildVersion"></span> built at <span id="buildTime"></span> on <span id="buildBranch"></span></div>
    <div>Copyright &copy; 2023<span id="curYear"></span> Dan Luca. All rights reserved.</div>
</footer>
</body>

</html>
)~~~";
