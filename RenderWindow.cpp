#include <QtEvents>
#include <QCoreApplication>
#include <QWindow>
#include <QQuickItem>
#include <QX11Info>
#include "RenderWindow.hpp"


RenderWindow::RenderWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    m_dpy = QX11Info::display();
    int screen=DefaultScreen(m_dpy);
    Visual *visual=DefaultVisual(m_dpy, screen);

    XRenderPictFormat* fmt = XRenderFindVisualFormat(m_dpy, visual);
    int depth = DefaultDepth(m_dpy, screen);

    show();
    m_wid = winId();

    m_background = XCreatePixmap(m_dpy, m_wid, 800, 600, depth);
    XSelectInput(m_dpy, m_wid, StructureNotifyMask);
    XSetWindowBackgroundPixmap(m_dpy, m_wid, m_background);
    XMapWindow(m_dpy, m_wid);

    XRenderPictureAttributes pict_attr;
    pict_attr.poly_edge=PolyEdgeSmooth;
    pict_attr.poly_mode=PolyModeImprecise;
    m_drawingarea = XRenderCreatePicture(m_dpy, m_background, fmt, CPPolyEdge|CPPolyMode,
                                         &pict_attr);

    installEventFilter(this);
}

void RenderWindow::refresh()
{
    Picture pic;

    XFlush(m_dpy);


    int realW = width()  * devicePixelRatioF();
    int realH = height() * devicePixelRatioF();

    if (m_videoClient) {
        pic = m_videoClient->getPicture();
        XRenderComposite(m_dpy, PictOpOver, pic, 0, m_drawingarea,
                         0,0, 0, 0,
                         0, 0, realW, realH);
    }

    if (m_interfaceClient) {
        pic = m_interfaceClient->getPicture();
        XRenderComposite(m_dpy, PictOpOver, pic, 0, m_drawingarea,
                         0,0, 0, 0,
                         0, 0, realW, realH);
    }

    XClearArea(m_dpy, m_wid, 0, 0, realW, realH, 0);
}

#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif

static void remapInputMethodQueryEvent(QObject *object, QInputMethodQueryEvent *e)
{
    auto item = qobject_cast<QQuickItem *>(object);
    if (!item)
        return;
    // Remap all QRectF values.
    for (auto query : {Qt::ImCursorRectangle, Qt::ImAnchorRectangle, Qt::ImInputItemClipRectangle})
    {
        if (e->queries() & query)
        {
            auto value = e->value(query);
            if (value.canConvert<QRectF>())
                e->setValue(query, item->mapRectToScene(value.toRectF()));
        }
    }
    // Remap all QPointF values.
    if (e->queries() & Qt::ImCursorPosition)
    {
        auto value = e->value(Qt::ImCursorPosition);
        if (value.canConvert<QPointF>())
            e->setValue(Qt::ImCursorPosition, item->mapToScene(value.toPointF()));
    }
}


bool RenderWindow::eventFilter(QObject*, QEvent* event)
{
    assert(m_interfaceWindow);
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Show:
        refresh();
        break;

    case QEvent::Resize:
    {
        //m_interfaceWindow->resize(size());
        refresh();
        break;
    }

    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::Leave:
        return QCoreApplication::sendEvent(m_interfaceWindow, event);
    case QEvent::Enter:
    {
        QEnterEvent *enterEvent = static_cast<QEnterEvent *>(event);
        QEnterEvent mappedEvent(enterEvent->localPos(), enterEvent->windowPos(),
                                enterEvent->screenPos());
        bool ret = QCoreApplication::sendEvent(m_interfaceWindow, &mappedEvent);
        event->setAccepted(mappedEvent.isAccepted());
        return ret;
    }

    case QEvent::InputMethod:
        return QCoreApplication::sendEvent(m_interfaceWindow->focusObject(), event);
    case QEvent::InputMethodQuery:
    {
        bool eventResult = QCoreApplication::sendEvent(m_interfaceWindow->focusObject(), event);
        // The result in focusObject are based on offscreenWindow. But
        // the inputMethodTransform won't get updated because the focus
        // is on QQuickWidget. We need to remap the value based on the
        // widget.
        remapInputMethodQueryEvent(m_interfaceWindow->focusObject(), static_cast<QInputMethodQueryEvent *>(event));
        return eventResult;
    }

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QMouseEvent mappedEvent(mouseEvent->type(), mouseEvent->localPos(),
                                mouseEvent->localPos(), mouseEvent->screenPos(),
                                mouseEvent->button(), mouseEvent->buttons(),
                                mouseEvent->modifiers(), mouseEvent->source());
        QCoreApplication::sendEvent(m_interfaceWindow, &mappedEvent);
        return true;
    }
    case QEvent::Wheel:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::HoverMove:

    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::DragLeave:
    case QEvent::DragResponse:
    case QEvent::Drop:
        return QCoreApplication::sendEvent(m_interfaceWindow, event);

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
        return QCoreApplication::sendEvent(m_interfaceWindow, event);


    default: break;
    }
    return false;
}
