#include <memory>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickView>
#include <QOpenGLContext>
#include <QQmlComponent>
#include <QWidget>
#include <QTimer>
#include <QX11Info>
#include <QOffscreenSurface>
#include <xcb/xcbext.h>
#include <xcb/render.h>
#include <xcb/damage.h>
#include <xcb/composite.h>


#include "RenderWindow.hpp"
#include "RenderClient.hpp"
#include "OffscreenQmlView.hpp"

#include <vlc/vlc.h>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    assert(QX11Info::isPlatformX11());

    RenderWindow server;
    if (!server.init()) {
        return -1;
    }

    OffscreenQmlView* qmlview = new OffscreenQmlView(server.windowHandle());
    QQmlEngine* engine = qmlview->engine();
    QQmlComponent* component = new QQmlComponent(engine, QStringLiteral("qrc:///main.qml"), QQmlComponent::PreferSynchronous, engine);
    QObject* rootObject = component->create();
    qmlview->setContent(component, (QQuickItem*)rootObject);
    qmlview->resize(800, 600);
    qmlview->setPosition(800, 600);
    qmlview->winId();
    qmlview->show();


    QWidget* videoWidget = new QWidget();
    videoWidget->setAttribute(Qt::WA_NativeWindow);
    videoWidget->setWindowFlag(Qt::WindowType::BypassWindowManagerHint);
    videoWidget->setWindowFlag(Qt::WindowType::WindowTransparentForInput);
    videoWidget->resize(800, 600);
    videoWidget->winId();
    videoWidget->show();

    libvlc_instance_t* vlc = libvlc_new(0, nullptr);
    auto m = libvlc_media_new_location(vlc, "file:///home/pierre/Videos/Doctor.Who.2005.S08E06.720p.HDTV.x265.mp4");
    libvlc_media_player_t* mp = libvlc_media_player_new_from_media(m);
    libvlc_media_release (m);
    libvlc_media_player_set_xwindow(mp, videoWidget->winId());
    libvlc_media_player_play (mp);

    RenderClient interfaceClient(qmlview);
    interfaceClient.show();
    server.setInterfaceClient(&interfaceClient, qmlview);

    RenderClient videoClient(videoWidget->windowHandle());
    videoClient.show();
    server.setVideoClient(&videoClient, videoWidget->windowHandle());

    server.show();

    app.exec();

    return 0;
}
