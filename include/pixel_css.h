const char pixel_css[] PROGMEM = R"~~~(
/* Set padding and margins to 0 then change later
Helps with cross browser support */
* {
    padding:2px;
    margin:0;
   }
   
header {
    background-color: #6666ff;
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

footer {
    color: #AAAAAA;
    text-align: center;
    font-size: 12pt;
    margin-top: 2em;
}

#effects, #time, #status, #settings {
    border: #afafaf solid 1px;
    border-radius: 10px;
    padding: 2em 1em;
    margin-top: 2em;
}

#effects #fxChangeArea, #time #timeChangeArea {
    width: 49%;
    float: left;
}

#effects #fxChangeArea #fxListLabel {
    color: #7f7f7f;
    display: block;
    padding: 5px 0;
}

#effects #fxChangeArea #fxAutoChangeArea, #curHolidayLabel, #holidayListLabel {
    color: #7f7f7f;
    padding: 1em 0;
}

#effects #fxChangeArea #fxlist {
    width: 75%;
    text-overflow: ellipsis;
}

#effects #curEffectArea, #time #curHolidayArea {
    margin-left: 50%;
}

#effects #curEffectArea label {
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

#settings {
    display: none;
}
)~~~";
