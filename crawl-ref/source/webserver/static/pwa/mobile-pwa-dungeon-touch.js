"use strict";

export function createDungeonTouch(options) {
    var $ = options.$;
    var touchElement = null;
    var cleanup = null;
    var press = null;
    var longPressTimer = null;
    var longPressDelay = options.longPressDelay || 425;
    var tapMoveSlop = options.tapMoveSlop || 14;

    function resetPress()
    {
        if (longPressTimer)
        {
            window.clearTimeout(longPressTimer);
            longPressTimer = null;
        }
        press = null;
    }

    function touchPoint(touch)
    {
        return {
            clientX: touch.clientX,
            clientY: touch.clientY,
            pageX: touch.pageX,
            pageY: touch.pageY
        };
    }

    function movedBeyondTapSlop(point)
    {
        if (!press || !point)
            return true;

        var dx = point.clientX - press.start.clientX;
        var dy = point.clientY - press.start.clientY;
        return dx * dx + dy * dy > tapMoveSlop * tapMoveSlop;
    }

    function updatePress(point)
    {
        if (!press || !point)
            return;

        press.latest = point;
        if (movedBeyondTapSlop(point))
            press.moved = true;
    }

    function clearLongPressTimer()
    {
        if (!longPressTimer)
            return;

        window.clearTimeout(longPressTimer);
        longPressTimer = null;
    }

    function triggerMouse(element, point, which)
    {
        if (!point)
            return;

        $(element).trigger($.Event("mousedown", {
            which: which,
            button: which === 3 ? 2 : 0,
            buttons: which === 3 ? 2 : 1,
            clientX: point.clientX,
            clientY: point.clientY,
            pageX: point.pageX,
            pageY: point.pageY
        }));
    }

    function startPress(element, point)
    {
        resetPress();
        press = {
            start: point,
            latest: point,
            moved: false,
            longPressed: false
        };

        longPressTimer = window.setTimeout(function () {
            longPressTimer = null;
            if (!press || press.moved)
                return;

            press.longPressed = true;
            triggerMouse(element, press.latest, 3);
        }, longPressDelay);
    }

    function finishPress(element)
    {
        if (!press)
            return;

        clearLongPressTimer();
        if (!press.longPressed && !press.moved)
            triggerMouse(element, press.latest, 1);

        resetPress();
    }

    function bind()
    {
        var dungeon = document.getElementById("dungeon");
        if (!dungeon)
            return;

        if (touchElement === dungeon)
            return;

        if (cleanup)
            cleanup();

        var boundDungeon = dungeon;
        touchElement = boundDungeon;

        function cancelEvent(event)
        {
            event.preventDefault();
        }

        function onTouchStart(event)
        {
            // Don't preventDefault on multi-touch: keeps pinch-zoom and
            // assistive-tech gestures available to the user.
            if (event.touches.length !== 1)
            {
                resetPress();
                return;
            }

            cancelEvent(event);
            startPress(boundDungeon, touchPoint(event.touches[0]));
        }

        function onTouchMove(event)
        {
            if (!press)
                return;

            if (event.touches.length !== 1)
            {
                resetPress();
                return;
            }

            cancelEvent(event);
            updatePress(touchPoint(event.touches[0]));
            if (press && press.moved)
                clearLongPressTimer();
        }

        function onTouchEnd(event)
        {
            if (!press)
                return;

            cancelEvent(event);
            if (event.changedTouches.length)
                updatePress(touchPoint(event.changedTouches[0]));

            if (event.touches.length === 0)
                finishPress(boundDungeon);
            else
                resetPress();
        }

        function onTouchCancel(event)
        {
            cancelEvent(event);
            resetPress();
        }

        function onContextMenu(event)
        {
            cancelEvent(event);
        }

        boundDungeon.addEventListener("touchstart", onTouchStart, { passive: false });
        boundDungeon.addEventListener("touchmove", onTouchMove, { passive: false });
        boundDungeon.addEventListener("touchend", onTouchEnd, { passive: false });
        boundDungeon.addEventListener("touchcancel", onTouchCancel, { passive: false });
        boundDungeon.addEventListener("contextmenu", onContextMenu);

        cleanup = function () {
            boundDungeon.removeEventListener("touchstart", onTouchStart);
            boundDungeon.removeEventListener("touchmove", onTouchMove);
            boundDungeon.removeEventListener("touchend", onTouchEnd);
            boundDungeon.removeEventListener("touchcancel", onTouchCancel);
            boundDungeon.removeEventListener("contextmenu", onContextMenu);
            resetPress();
            cleanup = null;
            touchElement = null;
        };
    }

    return {
        bind: bind,
        cleanup: function () {
            if (cleanup)
                cleanup();
            else
                resetPress();
        }
    };
}
