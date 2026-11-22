// Compare against the HTML version, including when firmware changes before the first poll.
let firmwareReloadPending = false;

function reloadForFirmwareUpdate(data) {
    const pageVersion = document.querySelector('meta[name="firmware-version"]')?.content;
    if (!pageVersion || !data.firmwareVersion || pageVersion === data.firmwareVersion) {
        return false;
    }
    if (!firmwareReloadPending) {
        firmwareReloadPending = true;
        const url = new URL(window.location.href);
        // A new URL also bypasses HTML cached by older firmware with immutable caching.
        url.searchParams.set('uiVersion', data.firmwareVersion);
        window.location.replace(url.href);
    }
    return true;
}
