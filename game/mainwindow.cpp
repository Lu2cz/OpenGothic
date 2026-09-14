#include "mainwindow.h"
#include "world/triggers/cscamera.h"
#include "world/objects/item.h"
#include "world/objects/interactive.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#if defined(__OSX__)
#include <objc/message.h>
#endif

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "ui/dialogmenu.h"
#include "ui/menuroot.h"
#include "ui/gamemenu.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "utils/mouseutil.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/aiqueue.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"

#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),renderer(swapchain),
    rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),
#if defined(__MOBILE_PLATFORM__)
    mobileUi(player),
#endif
    player(dialogs,inventory) {
  Gothic::inst().onSettingsChanged.bind(this,&MainWindow::onSettings);
  onSettings();

  if(Gothic::inst().version().game==2)
    setWindowTitle("Gothic II"); else
    setWindowTitle("Gothic");

  if(!CommandLine::inst().isWindowMode())
    setFullscreen(true);

  //renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  Gothic::inst().onStartGame   .bind(this,&MainWindow::startGame);
  Gothic::inst().onLoadGame    .bind(this,&MainWindow::loadGame);
  Gothic::inst().onSaveGame    .bind(this,&MainWindow::saveGame);

  Gothic::inst().onStartLoading.bind(this,&MainWindow::onStartLoading);
  Gothic::inst().onWorldLoaded .bind(this,&MainWindow::onWorldLoaded);
  Gothic::inst().onSessionExit .bind(this,&MainWindow::onSessionExit);

  Gothic::inst().onVideo       .bind(this,&MainWindow::onVideo);

  Gothic::inst().onBenchmarkFinished.bind(this,&MainWindow::onBenchmarkFinished);

  if(!Gothic::inst().defaultSave().empty()){
    Gothic::inst().load(Gothic::inst().defaultSave());
    rootMenu.popMenu();
    }
  else if(!CommandLine::inst().doStartMenu()) {
    startGame(Gothic::inst().defaultWorld());
    rootMenu.popMenu();
    }
  else {
    rootMenu.processMusicTheme();
    }

  funcKey[2] = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  funcKey[2].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F2>);

  funcKey[3] = Shortcut(*this,Event::M_NoModifier,Event::K_F3);
  funcKey[3].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F3>);

  funcKey[4] = Shortcut(*this,Event::M_NoModifier,Event::K_F4);
  funcKey[4].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F4>);

  funcKey[5] = Shortcut(*this,Event::M_NoModifier,Event::K_F5);
  funcKey[5].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F5>);

  funcKey[6] = Shortcut(*this,Event::M_NoModifier,Event::K_F6);
  funcKey[6].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F6>);

  funcKey[7] = Shortcut(*this,Event::M_NoModifier,Event::K_F7);
  funcKey[7].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F7>);

  funcKey[9] = Shortcut(*this,Event::M_NoModifier,Event::K_F9);
  funcKey[9].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F9>);

  funcKey[10] = Shortcut(*this,Event::M_NoModifier,Event::K_F10);
  funcKey[10].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F10>);

  displayPos = Shortcut(*this,Event::M_Alt,Event::K_P);
  displayPos.onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_P>);
  }

MainWindow::~MainWindow() {
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  takeWidget(&mobileUi);
#endif
  removeAllWidgets();
  // unload
  Gothic::inst().setGame(std::unique_ptr<GameSession>());
  }

float MainWindow::uiScale() const {
  return SystemApi::uiScale(hwnd());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  addWidget(&mobileUi);
#endif

  rootMenu.setMainMenu();

  Gothic::inst().onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  Gothic::inst().isNpcInDialogFn = std::bind(&DialogMenu::isNpcInDialog, &dialogs, std::placeholders::_1);

  Gothic::inst().onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  Gothic::inst().onPrint       .bind(&dialogs,&DialogMenu::print);

  Gothic::inst().onIntroChapter.bind(&chapter, &ChapterScreen::show);
  Gothic::inst().onShowDocument.bind(&document,&DocumentMenu::show);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);
  auto world = Gothic::inst().world();
  auto st    = Gothic::inst().checkLoading();

  if(!Gothic::inst().isInGame() && st==Gothic::LoadState::Idle && background.isEmpty()) {
    background = Resources::loadTextureUncached("STARTSCREEN.TGA");
    if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_KMLIB_MENU_PROBE")!=nullptr)
      Log::i("[KMLIB_PROBE] background=",background.w(),"x",background.h());
    }

  if(world==nullptr && !background.isEmpty()) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle) {
      p.setBrush(Brush(background,Painter::NoBlend));
      p.drawRect(0,0,w(),h(),
                 0,0,background.w(),background.h());
      }
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  if(st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(auto back = Gothic::inst().loadingBanner()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        }
      if(loadBox!=nullptr && !loadBox->isEmpty()) {
        if(Gothic::inst().version().game==1) {
          int lw = int(w()*0.5);
          int lh = int(h()*0.05);
          drawLoading(p,(w()-lw)/2, int(h()*0.75), lw, lh);
          } else {
          drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
          }
        }
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *Gothic::inst().camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      drawMsg(p);

      auto focus = world->validateFocus(player.focus());
      if(std::getenv("OPENGOTHIC_PROFILE") && std::getenv("OPENGOTHIC_BOSS_UI_PROBE")) {
        if(auto target=std::getenv("OPENGOTHIC_BOSS_UI_FOCUS")) {
          focus = Focus();
          if(std::string_view(target)!="none") {
            auto& vm = world->script().getVm();
            auto* npc = world->findNpcByInstance(vm.find_symbol_by_name(
                std::string_view(target)=="boss" ? "RAZOR_ARMORED" : "VLK_3015_DETLOW")->index());
            if(npc)
              focus = Focus(*npc);
            }
          }
        }
      if(auto pl = Gothic::inst().player())
        world->script().setNpcFocus(*pl,focus.npc);

      paintFocus(p,focus,vp);

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop()) {
          auto& opt = Gothic::options();
          float hp  = float(pl->attribute(ATR_HITPOINTS))/float(pl->attribute(ATR_HITPOINTSMAX));
          float mp  = float(pl->attribute(ATR_MANA))     /float(pl->attribute(ATR_MANAMAX));

          bool showHealthBar = opt.showHealthBar;
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

          if(showHealthBar)
            drawBar(p,barHp, 10, h()-10, hp, AlignLeft | AlignBottom);
          if(showManaBar)
            drawBar(p,barMana, w()-10, h()-10, mp, AlignRight | AlignBottom);
          if(showSwimBar) {
            uint32_t gl = pl->guild();
            auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
            if(v>0) {
              auto t = float(pl->diveTime())/1000.f;
              drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
              }
            }
          }
        }
      world->script().drawUi(p,w(),h(),statusBarScale());
      }
    }

  if(auto c = Gothic::inst().camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      world->view()->dbgLights(dbg);
      world->marchCsCameras(dbg);
      }
    }

  renderer.dbgDraw(p);

  const float scale = Gothic::interfaceScale(this);
  if(Gothic::inst().doFrate() && !Gothic::inst().isDesktop()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

    auto& fnt = Resources::font(scale);
    fnt.drawText(p,5,fnt.pixelSize()+5,fpsT);
    }

  if(!Gothic::inst().isDesktop() && world!=nullptr) {
    if(Gothic::inst().doClock()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
      }

    auto c = Gothic::inst().camera();
    if(Gothic::inst().doVobBox() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      world->drawVobBoxNpcNear(dbg);
      }

    if(Gothic::inst().doVobRays() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      player.drawVobRay(dbg);
      }
    }

  if(auto wx = Gothic::inst().worldView()) {
    wx->dbgClusters(p, Vec2(float(w()), float(h())));
    }
  }

void MainWindow::resizeEvent(SizeEvent&) {
  for(auto& i:fence)
    i.wait();
  swapchain.reset();
  renderer.resetSwapchain();
  if(auto camera = Gothic::inst().camera())
    camera->setViewport(swapchain.w(),swapchain.h());

  const bool fs = SystemApi::isFullscreen(hwnd());
  auto rect = SystemApi::windowClientRect(hwnd());
  setCursorPosition(rect.w/2,rect.h/2);
  setCursorShape(fs ? CursorShape::Hidden : CursorShape::Arrow);
  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
  auto center = Point(w()/2,h()/2);
  if(enable && event.pos()!=center && hasFocus()) {
    dMouse += (event.pos()-center);
    setCursorPosition(center);
    }
  }

void MainWindow::tickMouse(uint64_t dt) {
  auto camera = Gothic::inst().camera();
  if(dialogs.hasContent() || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = Gothic::inst().settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  if(dMouse==Point())
    return;

  const bool  camLookaroundInverse = Gothic::inst().settingsGetI("GAME","camLookaroundInverse");
  const float mouseSensitivity     = Gothic::inst().settingsGetF("GAME","mouseSensitivity")/MouseUtil::mouseSysSpeed();
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());
  if(camLookaroundInverse)
    dpScaled.y *= -1.f;

  static float mul = 270.f;
  dpScaled *= mul;

  static float psMax = 720.f;
  const float  dtF   = float(dt)/1000.f;
  dpScaled.x = std::clamp(dpScaled.x, -(psMax*dtF), psMax*dtF);
  dpScaled.y = std::clamp(dpScaled.y, -(psMax*dtF), psMax*dtF);

  // Log::d("mouse dMouse   = ", dMouse.x,   ", ", dMouse.y);
  // Log::d("mouse dpScaled = ", dpScaled.x, ", ", dpScaled.y);

  camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse(-dpScaled.x, -dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::onSettings() {
  auto zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(auto camera = Gothic::inst().camera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(video.isActive()){
    event.accept();
    video.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&video;
      return;
      }
    }

  if(rootMenu.isActive()) {
    event.accept();
    rootMenu.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&rootMenu;
      return;
      }
    }

  if(chapter.isActive()){
    event.accept();
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(document.isActive()){
    event.accept();
    document.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&document;
      return;
      }
    }

  if(dialogs.isActive()){
    event.accept();
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    event.accept();
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;

  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto tex = renderer.screenshoot(cmdId);
    auto pm  = device.readPixels(textureCast<const Texture2d&>(tex));
    pm.save("dbg.png");
    }
  event.accept();
  }

void MainWindow::keyRepeatEvent(KeyEvent& event) {
  if(uiKeyUp==&video){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    rootMenu.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  if(uiKeyUp==&video){
    video.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    document.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }


  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);

  std::string_view menuEv;
  if(act==KeyCodec::Escape)
    menuEv = Gothic::inst().menuMain();
  else if(act==KeyCodec::Log)
    menuEv = "MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv = "MENU_STATUS";

  if(!menuEv.empty()) {
    rootMenu.setMenu(menuEv,act);
    rootMenu.showVersion(act==KeyCodec::Escape);
    if(auto pl = Gothic::inst().player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory && !dialogs.isActive()) {
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = Gothic::inst().player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  player.onKeyReleased(act, mapping);
  }

void MainWindow::focusEvent(FocusEvent &event) {
  if(!event.in)
    return;
  dMouse = Point();
  auto center = Point(w()/2,h()/2);
  setCursorPosition(center);
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto        world = Gothic::inst().world();
  auto        pl    = world==nullptr ? nullptr : world->player();
  if(pl==nullptr)
    return;

  auto pw  = 1.f;
  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);

  if(pw<=0.f)
    return;

  pos /= pw;

  int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font(scale);

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  fnt.drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,world->script().focusBarY(h()), hp, AlignHCenter|AlignTop);
    }

  const int foc = Gothic::settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto tr = vp;
    tr.mul(focus.npc->transform());

    const auto b    = focus.npc->bounds();
    const auto bbox = b.bbox; //focus.npc->bBox();

    Vec3 bx[] = {
      {bbox[0].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[1].y, bbox[1].z},
      {bbox[0].x, bbox[1].y, bbox[1].z},
      };

    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      tr.project(bx[i]);
      int x = int((bx[i].x*0.5f+0.5f)*float(w()));
      int y = int((bx[i].y*0.5f+0.5f)*float(h()));
      min[0] = std::min(x,min[0]);
      min[1] = std::min(y,min[1]);
      max[0] = std::max(x,max[0]);
      max[1] = std::max(y,max[1]);
      }

    paintFocus(p,Rect(min[0],min[1],max[0]-min[0],max[1]-min[1]));
    }

  // focusImg
  /*
  if(auto pl = focus.interactive){
    pl->marchInteractives(p,vp,w(),h());
    }*/
  }

void MainWindow::paintFocus(Painter& p, Rect rect) {
  if(focusImg==nullptr)
    return;
  const int w2 = focusImg->w();
  const int h2 = focusImg->h();
  const int w  = w2/2;
  const int h  = h2/2;

  if(rect.w<w) {
    int dw = w-rect.w;
    rect.x -= dw/2;
    rect.w += dw;
    }
  if(rect.h<h) {
    int dh = h-rect.h;
    rect.y -= dh/2;
    rect.h += dh;
    }

  p.setBrush(Brush(*focusImg,Painter::Add));
  p.drawRect(rect.x,         rect.y,         w,h, 0,0, w, h);
  p.drawRect(rect.x+rect.w-w,rect.y,         w,h, w,0, w2,h);
  p.drawRect(rect.x,         rect.y+rect.h-h,w,h, 0,h, w, h2);
  p.drawRect(rect.x+rect.w-w,rect.y+rect.h-h,w,h, w,h, w2,h2);
  }

float MainWindow::statusBarScale() const {
  // Status bars share an 800x600 layout in framebuffer pixels. Scaling it to
  // the viewport keeps script virtual positions and pixel-sized bars coherent.
  // The canvas also reserves vertical room for the script's fixed focus offset.
  // Status-bar zoom saturates at fit; smaller user multipliers still shrink it.
  const float fit = std::min(float(std::max(w(),1))/800.f,float(std::max(h(),1))/600.f);
  return fit*std::min(Gothic::options().interfaceScale,1.f);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float destW   = 200.f*statusBarScale();
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*24.f/32.f;
  //const float destHin = 20;//float(destH)*24.f/32.f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  if(std::getenv("OPENGOTHIC_PROFILE") && std::getenv("OPENGOTHIC_BOSS_UI_PROBE"))
    Log::i("[BOSS_UI] native bar=",(flg & AlignTop) ? "focus" : "player"," rect=",x,",",y,",",int(destW),",",int(destH));

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*(destH-destHin));
  float pd = 9.f*k;
  if(std::getenv("OPENGOTHIC_PROFILE") && std::getenv("OPENGOTHIC_BOSS_UI_PROBE"))
    Log::i("[BOSS_UI] native fill=",(flg & AlignTop) ? "focus" : "player",
           " rect=",x+int(pd),",",y+dy,",",int(float(destW-pd*2)*v),",",int(destHin));
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(destHin),
             0,0,bar->w(),bar->h());
  }

void MainWindow::drawMsg(Tempest::Painter& p) {
  const float destW   = 200.f*statusBarScale();
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;

  const int y = 10 + int(destH) + 10;
  dialogs.drawMsg(p, y);
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  int paddL = int((float(w)*75.f)/float(loadBox->w()));
  int paddT = int((float(h)*10.f)/float(loadBox->h()));

  if(Gothic::inst().version().game==1) {
    paddL = int((float(w)*30.f)/float(loadBox->w()));
    paddT = int((float(h)* 5.f)/float(loadBox->h()));
    }

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = Gothic::inst().loadingBanner()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  int         szX   = Gothic::options().saveGameImageSize.w;
  int         szY   = Gothic::options().saveGameImageSize.h;

  if(szX<=460 || szY<=300) {
    // way too small otherwise
    szX = 460;
    szY = 300;
    }
  szX = int(float(szX)*scale);
  szY = int(float(szY)*scale);
  drawSaving(p,*saveback,szX,szY,scale);
  }

void MainWindow::drawSaving(Painter& p, const Tempest::Texture2d& back, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(back);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh, 0,0,back.w(),back.h());
    }

  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive());
  }

template<Tempest::KeyEvent::KeyType k>
void MainWindow::onMarvinKey() {
  switch(k) {
    case Event::K_F2:
      if(Gothic::inst().isMarvinEnabled()) {
        console.resize(w(),h());
        console.setFocus(true);
        console.exec();
        }
      break;
    case Event::K_F3:
      setFullscreen(!SystemApi::isFullscreen(hwnd()));
      break;
    case Event::K_F4:
      if(Gothic::inst().isMarvinEnabled()) {
        auto camera = Gothic::inst().camera();
        auto pl = Gothic::inst().player();
        if(camera!=nullptr && pl!=nullptr) {
          camera->setMarvinMode(Camera::M_Normal);
          camera->reset(pl);
          }
        }
      break;
    case Event::K_F5: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
#ifdef NDEBUG
      const bool debug = false;
#else
      const bool debug = true;
#endif
      if(!debug && Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Freeze);
          }
        }
      else if(Gothic::inst().isInGameAndAlive() && !Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickSave();
        }
      break;
      }

    case Event::K_F6:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        auto camera = Gothic::inst().camera();
        auto pl     = Gothic::inst().player();
        auto inter  = pl!=nullptr ? pl->interactive() : nullptr;
        if(camera!=nullptr && inter==nullptr)
          camera->setMarvinMode(Camera::M_Free);
        }
      break;
    case Event::K_F7:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Pinned);
          }
        }
      break;
    case Event::K_F8:
      //player.marvinF8();
      break;

    case Event::K_F9: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
      if(Gothic::inst().isMarvinEnabled()) {
        if(runtimeMode==R_Normal)
          runtimeMode = R_Suspended; else
          runtimeMode = R_Normal;
        }
      else if(!Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickLoad();
        }
      break;
      }
    case Event::K_F10:
      if(runtimeMode==R_Suspended)
        runtimeMode = R_Step;
      break;
    case Event::K_P:
      if(Gothic::inst().isMarvinEnabled()) {
        if(auto p = Gothic::inst().player()) {
          auto pos = p->position();
          string_frm buf("Position: ", pos.x,'/',pos.y,'/',pos.z);
          Gothic::inst().onPrint(buf);
          }
        }
      break;
    }
  }

uint64_t MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  // NOTE: limit to ~200 FPS in game logic to avoid math issues
  if(dt<5)
    return 0;
  lastTick  = time;
  GameMusic::inst().tick();

  auto st = Gothic::inst().checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    Gothic::inst().finishLoading();
    if(st==Gothic::LoadState::FailedLoad)
      rootMenu.setMainMenu();
    if(st==Gothic::LoadState::FailedSave)
      Gothic::inst().onPrint("unable to write savegame file");
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      rootMenu.processMusicTheme();
    return 0;
    }

  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_KMLIB_MENU_PROBE")!=nullptr &&
     !Gothic::inst().isInGame()) {
    static unsigned stage = 0;
    static uint64_t menuAt = 0;
    if(menuAt==0)
      menuAt = time;
    if(time-menuAt>4000) {
      Log::i("[KMLIB_PROBE] menu stage=",stage);
      GameMusic::inst().traceFileMusic();
      menuAt=0;
      if(stage++==0) {
        Gothic::inst().load("save_slot_1.sav");
        rootMenu.popMenu();
        } else {
        Tempest::SystemApi::exit();
        }
      }
    }

  video.tick();
  if(video.isActive())
    return 0;

  if(Gothic::inst().isPause()) {
    return 0;
    }

  if(dt>50)
    dt=50;

  if(runtimeMode==R_Step) {
    runtimeMode = R_Suspended;
    dt = 1000/60; //60 fps
    }
  else if(runtimeMode==R_Suspended) {
    auto camera = Gothic::inst().camera();
    if(camera!=nullptr && camera->isFree()) {
      tickMouse(dt);
      }
    update();
    return 0;
    }

  dialogs.tick(dt);
  inventory.tick(dt);
  Gothic::inst().tick(dt);
  player.tickFocus();

  if(dialogs.isActive())
    ;//clearInput();
  if(document.isActive())
    clearInput();
  tickMouse(dt);
  player.tickMove(dt);
  update();
  return dt;
  }

void MainWindow::updateAnimation(uint64_t dt) {
  Gothic::inst().updateAnimation(dt);
  }

void MainWindow::tickCamera(uint64_t dt) {
  auto pcamera = Gothic::inst().camera();
  auto pl      = Gothic::inst().player();
  if(pcamera==nullptr)
    return;

  auto&      camera       = *pcamera;
  const auto ws           = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos          = pl!=nullptr ? pl->cameraBone(camera.isFirstPerson()) : Vec3();

  if(!camera.isCutscene() && !camera.isFree()) {
    const bool fs = SystemApi::isFullscreen(hwnd());
    if(!fs && mouseP[Event::ButtonLeft]) {
      camera.setSpin(camera.spin());
      camera.setTarget(pos);
      }
    else if(dialogs.isActive() && !dialogs.isMobsiDialog()) {
      dialogs.dialogCamera(camera);
      }
    else if(inventory.isActive()) {
      camera.setTarget(pos);
      }
    else if(player.focus().npc!=nullptr && meleeFocus && pl!=nullptr) {
      auto spin = camera.spin();
      spin.y = pl->rotation();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    else if(pl!=nullptr && !camera.isFree()) {
      auto spin = camera.spin();
      if(pl->interactive()==nullptr && !pl->isDown())
        spin.y = pl->rotation();
      if(pl->isDive() && !camera.isMarvin())
        spin.x = -pl->rotationY();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    }

  if(dt==0)
    return;
  if(camera.isToggleEnabled() && !camera.isCutscene())
    camera.setMode(solveCameraMode());
  camera.tick(dt);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  const auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isFree())
    return Camera::Normal;

  if(dialogs.isActive())
    return Camera::Dialog;

  if(camera!=nullptr && camera->isFirstPerson())
    return Camera::FirstPerson;

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(auto pl = Gothic::inst().player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    if(pl->isFallingDeep())
      return Camera::Fall;
    bool g2 = Gothic::inst().version().game==2;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return g2 ? Camera::Ranged : Camera::Normal;
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(std::string_view slot) {
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(slot));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::loadGame(std::string_view slot) {
  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().setBenchmarkMode(Benchmark::None);
  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(slot);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(s));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::saveGame(std::string_view slot, std::string_view name) {
  if(std::getenv("OPENGOTHIC_PROFILE")!=nullptr && std::getenv("OPENGOTHIC_UI_PROBE")!=nullptr)
    Log::i("[MODAL_PROBE] save requested slot=",slot," name=",name," dialogue=",dialogs.isActive());
  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;

  auto tex  = renderer.screenshoot(cmdId);
  auto lres = Attachment();

  static int32_t kThumbW = 800;
  const  int32_t kThumbH = tex.w()>0 ? int32_t((tex.h() * kThumbW) / tex.w()) : 0;
  if(kThumbW>0 && kThumbH>0 && kThumbW<tex.w() && kThumbH<tex.h()) {
    lres = device.attachment(Tempest::TextureFormat::RGBA8, uint32_t(kThumbW), uint32_t(kThumbH));
    }

  if(!lres.isEmpty()) {
    // reduce size of the save entry preview screenshot for faster save & load
    CommandBuffer cmd;
    {
    auto enc = cmd.startEncoding(device);
    enc.setDebugMarker("Downscale screenhoot");
    enc.setFramebuffer({{lres, Vec4(), Tempest::Preserve}});
    enc.setPushData(IVec2(lres.w(), lres.h()));
    enc.setBinding(0, tex, Sampler::nearest());
    enc.setPipeline(Shaders::inst().downscale);
    enc.draw(nullptr, 0, 3);
    }
    auto sync = device.submit(cmd);
    sync.wait();
    }

  auto& thumb = lres.isEmpty() ? tex : lres;
  auto  pm    = device.readPixels(textureCast<const Texture2d&>(thumb));

  Gothic::inst().startSave(std::move(textureCast<Texture2d&>(tex)),[slot=std::string(slot),name=std::string(name),pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    // Keep the previous slot intact if serialization rejects unsupported state.
    const auto temporary = slot+".tmp";
    try {
      {
      Tempest::WFile f(temporary);
      {
      Serialize s(f);
      if(auto mode=std::getenv("OPENGOTHIC_AI_WAIT_PROBE"); mode!=nullptr && std::string_view(mode)=="legacy-nav-seed")
        s.setVersion(55);
      game->save(s,name,pm);
      }
      if(!f.flush()) throw std::runtime_error("unable to flush savegame file");
      }
      std::filesystem::rename(temporary,slot);
      } catch(...) {
      std::error_code ignored;
      std::filesystem::remove(temporary,ignored);
      throw;
      }

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

void MainWindow::onVideo(std::string_view fname) {
  if(Gothic::inst().isBenchmarkMode())
    return;
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
  dMouse = Point();

  if(Gothic::inst().isBenchmarkMode()) {
    if(auto world = Gothic::inst().world()) {
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->execTriggerEvent(evt);
      }
    benchmark.clear();
    }

  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  device.waitIdle();
  for(auto& c:commands)
    c = device.commandBuffer();

  if(auto c = Gothic::inst().camera())
    c->setViewport(uint32_t(w()),uint32_t(h()));

  renderer.onWorldChanged();

  if(auto pl = Gothic::inst().player())
    rootMenu.setPlayer(*pl);

  if(auto pl = Gothic::inst().player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
  }

void MainWindow::onSessionExit() {
  rootMenu.setMainMenu();
  rootMenu.processMusicTheme();
  }

void MainWindow::onBenchmarkFinished() {
  if(benchmark.numFrames==0)
    return;

  double fps  = benchmark.fpsSum/double(benchmark.numFrames);
  double low1 = 0;
  size_t num1 = 0;
  for(size_t i=0; i<benchmark.low1procent.size(); ++i) {
    auto v = benchmark.low1procent[i];
    if(v<=0)
      continue;
    low1 += 1000.0/double(v);
    num1 += 1;
    }
  low1 = num1>0 ? low1/double(num1) : 0.0;
  benchmark.clear();

  string_frm str("Benchmark: low 1% = ", low1, " fps = ", fps);
  Log::i(str.c_str());
  console.printLine(str);

  if(Gothic::inst().isBenchmarkModeCi()) {
    Log::i("Exiting benchmark");
    Tempest::SystemApi::exit();
    return;
    }

  console.setFocus(true);
  console.exec();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  }

void MainWindow::render(){
  try {
    // Temporary local profiling: load a save, warm up, measure, then exit.
    static const bool profileEnabled = std::getenv("OPENGOTHIC_PROFILE")!=nullptr;
    static double loadedAt=0, profileAt=0, profileMs[7]={};
    static uint32_t profileFrames=0, profileSkipped=0;
    static uint32_t recipeRereadFrame=0;
    static Npc* dialogProbeNpc=nullptr;
    static Interactive* lockProbeTarget=nullptr;
    static size_t lockProbeScroll=0, lockProbeCount=0;
    static int lockProbeMana=0;
    static int lockProbeDexterity=0;
    static bool lockProbeSaved=false;
    static uint32_t lockProbeActionFrame=uint32_t(-1);
    static unsigned dialogProbeRounds=0;
    static bool dialogProbePending=false;
    static bool captainProbeSaved=false;
    static bool captainFixtureSaved=false;
    static bool beachProbeSaved=false;
    static bool cityProbeSaved=false;
    static bool worldProbeTransitioned=false;
    static bool worldProbeSaved=false;
    static bool worldProbeWalked=false;
    static uint32_t worldProbeWalkFrame=0;
    static unsigned musicProbeStage=0;
    static double musicProbeAt=0;
    static Tempest::Vec3 cityWalkStart;
    static Tempest::Vec3 worldProbeWalkStart;
    static double cityMeasuredAt=0;
    static bool forestProbeSaved=false;
    static bool aiWaitProbeSaved=false;
    static bool bossUiProbeStarted=false;
    static bool bossUiProbeSaved=false;
    static bool bossUiProbeHealth=false;
    static bool bossUiProbeFinished=false;
    static bool bossUiProbeCleaned=false;
    static uint32_t bossUiGeometryFrame=uint32_t(-1);
    static uint64_t aiWaitProbeTicket=0;
    static uint8_t aiWaitProbeStep=0;
    static size_t beachTorchCount=0;
    static Item* beachTorch=nullptr;
    static float beachTorchStartY=0;
    const auto profileNow = [] {
      return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
      };
    const double profileEntry = profileEnabled ? profileNow() : 0;
    const auto bossMode = std::getenv("OPENGOTHIC_BOSS_UI_PROBE");
    const bool bossGeometry = bossMode && std::string_view(bossMode)=="event-geometry";
    const bool profileReady = profileEnabled && Gothic::inst().world()!=nullptr &&
                              Gothic::inst().checkLoading()==Gothic::LoadState::Idle;
    const auto aiWaitMode = std::getenv("OPENGOTHIC_AI_WAIT_PROBE");
    const bool legacyNavProbe = aiWaitMode!=nullptr && std::string_view(aiWaitMode).starts_with("legacy-nav");
    if(profileReady && loadedAt==0) {
      if(bossGeometry)
        setFullscreen(false); // Native window restoration can reopen the previous fullscreen state.
#if defined(__OSX__)
      if(bossMode && std::getenv("OPENGOTHIC_BOSS_UI_1024")) {
        // Resize the real Cocoa content surface, in backing pixels, not the widget alone.
        struct CocoaSize { double width, height; }; // 64-bit NSSize ABI.
        auto window = reinterpret_cast<::id>(hwnd());
        const auto dpi = reinterpret_cast<double(*)(::id,SEL)>(objc_msgSend)(window,sel_registerName("backingScaleFactor"));
        reinterpret_cast<void(*)(::id,SEL,CocoaSize)>(objc_msgSend)(window,sel_registerName("setContentSize:"),CocoaSize{1024./dpi,768./dpi});
        }
#endif

      if(std::getenv("OPENGOTHIC_GATE_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_GATE_PROBE"))=="open")
        Gothic::inst().world()->execTriggerEvent(TriggerEvent("SHIP_TRAPDOOR", "", TriggerEvent::T_Trigger));
      loadedAt = profileEntry;
      }
    if(profileReady && (std::getenv("OPENGOTHIC_RECIPE_PROBE")!=nullptr ||
                        std::getenv("OPENGOTHIC_CITY_PROBE")!=nullptr) &&
       (video.isActive() || chapter.isActive())) {
      Log::i("[RECIPE_PROBE] dismiss opening video=",video.isActive()," chapter=",chapter.isActive());
      KeyEvent escape(Event::K_ESCAPE);
      keyDownEvent(escape);
      keyUpEvent(escape);
      loadedAt = profileEntry;
      }
    bool sampling = profileReady && (legacyNavProbe || profileEntry-loadedAt>=10000);
    if(sampling && profileAt==0) {
      if(auto mode=std::getenv("OPENGOTHIC_CITY_PROBE")) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        auto* pl = w.player();
        if(std::string_view(mode)=="create") {
          Log::i("[CITY_PROBE] begin story-helper preset");
          vm.find_symbol_by_name("STORYHELPERMAINSTORYLINE_CHAPTER2")->set_int(2);
          w.script().invokeState(pl->handlePtr(),pl->handlePtr(),"STORYHELPER_MAINSTORYLINE_CHAPTER2_COMMON");
          auto* wp = w.findPoint("PARTM2_MARKET_06",false);
          if(wp==nullptr)
            throw std::runtime_error("City market waypoint missing");
          pl->clearAiQueue();
          pl->clearState(true);
          pl->clearGoTo();
          pl->setInteraction(nullptr);
          pl->setPosition(wp->position());
          pl->setDirection(wp->direction());
          pl->updateTransform();
          Gothic::inst().camera()->reset(pl);
          w.setDayTime(12,0);
          }
        Log::i("[CITY_PROBE] ready world=",w.name()," chapter=",vm.find_symbol_by_name("KAPITEL")->get_int(),
               " entered=",vm.find_symbol_by_name("CITYENTERED")->get_int());
        }
      if(auto mode=std::getenv("OPENGOTHIC_BEACH_PROBE")) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        if(std::string_view(mode)=="repair") {
          auto* n = w.findNpcByInstance(vm.find_symbol_by_name("NONE_3_EZEKIEL")->index());
          auto* wp = n->currentTaPoint();
          if(wp==nullptr || wp->name!="PART_13_DARRYL_DEAD" ||
             vm.find_symbol_by_name("Q101_CAPTAIN_CUTSCENEENABLE")->get_int()!=11)
            throw std::runtime_error("Beach recovery requires Ezekiel's existing Pray routine");
          n->resumeAiRoutine();
          n->attachToPoint(nullptr);
          n->setPosition(wp->position());
          n->setDirection(wp->direction());
          Log::i("[BEACH_PROBE] recovered Ezekiel's existing routine");
          for(auto entry : {std::pair{"ITSC_LIGHTHEAL",size_t(1)},std::pair{"ITMI_GOLD",size_t(13)}}) {
            auto id=vm.find_symbol_by_name(entry.first)->index();
            Interactive* corpse=nullptr;
            for(uint32_t i=0;auto mob=w.mobsiById(i);++i)
              if(mob->tag()=="Q101_URS_BODY") {
                if(corpse!=nullptr)
                  throw std::runtime_error("Ambiguous corpse recovery target");
                corpse=mob;
                }
            if(corpse==nullptr || corpse->inventory().itemCount(id)!=0)
              throw std::runtime_error("Corpse recovery would duplicate an item");
            corpse->inventory().addItem(id,entry.second,w);
            }
          }
        for(uint32_t i=0;auto mob=w.mobsiById(i);++i) {
          if(mob->tag()!="Q101_URS_BODY")
            continue;
          for(auto it=mob->inventory().iterator(Inventory::T_Inventory);it.isValid();++it)
            Log::i("[BEACH_PROBE] corpse item=",vm.find_symbol_by_index(uint32_t(it->clsId()))->name()," count=",it.count());
          }
        if(std::string_view(mode)!="fresh") {
          auto id=vm.find_symbol_by_name("ITLSTORCHBURNING")->index();
          while(w.findItemByInstance(id,beachTorchCount)!=nullptr)
            ++beachTorchCount;
          auto& pl = *w.player();
          pl.closeWeapon(true);
          pl.setTorch(true);
          Log::i("[BEACH_PROBE] torch equipped=",pl.isUsingTorch());
          }
        }
      if(std::getenv("OPENGOTHIC_FOREST_PROBE")!=nullptr) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        auto jorn = w.findNpcByInstance(vm.find_symbol_by_name("NONE_1_JORN")->index());
        auto fabio = w.findNpcByInstance(vm.find_symbol_by_name("NONE_5_FABIO")->index());
        auto wp = w.findPoint("PART_13_NAV_11",false);
        jorn->clearGoTo();
        jorn->setPosition(wp->position());
        fabio->clearGoTo();
        fabio->setPosition(wp->position()+Tempest::Vec3(100,0,0));
        w.player()->setPosition(wp->position()+Tempest::Vec3(0,0,120));
        fabio->startDialog(*w.player());
        Log::i("[FOREST_PROBE] started Fabio dialogue");
        }
      if(auto mode=std::getenv("OPENGOTHIC_AI_WAIT_PROBE")) {
        auto& w = *Gothic::inst().world();
        auto& self = *w.player();
        auto* target = w.findNpcByInstance(w.script().getVm().find_symbol_by_name("NONE_1_JORN")->index());
        auto* other = w.findNpcByInstance(w.script().getVm().find_symbol_by_name("NONE_5_FABIO")->index());
        if(target==nullptr || other==nullptr || dialogs.isActive() || w.currentCs()!=nullptr)
          throw std::runtime_error("AI wait persistence probe needs normal world control");
        if(std::string_view(mode)=="seed") {
          self.clearAiQueue();
          target->clearAiQueue();
          target->aiPush(AiQueue::aiWait(16000));
          self.aiPush(AiQueue::aiWaitTillEnd(*target,target->aiWaitTicket()));
          Log::i("[AI_WAIT_PROBE] seeded self_empty=",self.isAiQueueEmpty()," target_busy=",target->isAiBusy());
          }
        if(std::string_view(mode)=="edges") {
          self.clearAiQueue();
          target->clearAiQueue();
          other->clearAiQueue();
          auto* point=w.findPoint("PART_13_NAV_11",false);
          if(point==nullptr)
            throw std::runtime_error("AI wait edge probe needs navigation point");
          self.setPosition(point->position()+Tempest::Vec3(0,0,120));
          target->setPosition(point->position());
          other->setPosition(point->position()+Tempest::Vec3(100,0,0));
          target->setProcessPolicy(NpcProcessPolicy::AiNormal);
          other->setProcessPolicy(NpcProcessPolicy::AiNormal);
          self.aiPush(AiQueue::aiWaitTillEnd(*target,target->aiWaitTicket()));
          self.aiPush(AiQueue::aiWaitTillEnd(self,self.aiWaitTicket()));
          target->aiPush(AiQueue::aiWait(1));
          aiWaitProbeTicket=target->aiWaitTicket();
          Log::i("[AI_WAIT_PROBE] edges started");
          }
        if(std::string_view(mode)=="legacy-nav-seed") {
          auto* point=w.findPoint("PART_13_NAV_11",false);
          if(point==nullptr)
            throw std::runtime_error("AI wait legacy navigation probe needs navigation point");
          self.clearAiQueue();
          target->clearAiQueue();
          target->setPosition(point->position()+Tempest::Vec3(0,0,300));
          target->setProcessPolicy(NpcProcessPolicy::AiNormal);
          target->aiPush(AiQueue::aiGoToPoint(*point));
          aiWaitProbeTicket=target->aiWaitTicket();
          Log::i("[AI_WAIT_PROBE] legacy navigation seeded ticket=",aiWaitProbeTicket);
          }
        if(std::string_view(mode)=="legacy-nav-reload") {
          self.clearAiQueue();
          aiWaitProbeTicket=target->aiWaitTicket();
          if(aiWaitProbeTicket==0)
            throw std::runtime_error("AI wait legacy navigation ticket was not restored");
          Log::i("[AI_WAIT_PROBE] legacy navigation restored ticket=",aiWaitProbeTicket);
          }
        }
      if(std::getenv("OPENGOTHIC_CAPTAIN_PROBE")!=nullptr) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        auto npc = w.findNpcByInstance(vm.find_symbol_by_name("NONE_1_JORN")->index());
        auto pl = w.player();
        if(std::string_view(std::getenv("OPENGOTHIC_CAPTAIN_PROBE"))=="prepare") {
          auto* point=w.findPoint("SHIP_JORN_02",false);
          if(npc==nullptr || point==nullptr || !w.script().probeCaptainFixture(*pl,*npc))
            throw std::runtime_error("Captain fixture requires the fresh ship scene");
          npc->clearAiQueue();
          npc->setPosition(point->position());
          pl->setPosition(point->position()+Tempest::Vec3(120,0,0));
          Log::i("[CAPTAIN_PROBE] fixture ready");
          captainFixtureSaved=true;
          saveGame("save_slot_2.sav","Captain fixture");
          } else {
          auto* point=w.findPoint("SHIP_JORN_02",false);
          if(npc==nullptr || point==nullptr)
            throw std::runtime_error("Captain probe requires the ship scene");
          npc->clearGoTo();
          npc->setPosition(point->position());
          pl->setPosition(point->position()+Tempest::Vec3(120,0,0));
          npc->startDialog(*pl);
          Log::i("[CAPTAIN_PROBE] started Jorn dialogue");
          }
        }
      if(std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        auto& pl = *w.player();
        Log::i("[STASH_PROBE] begin flag=",vm.find_symbol_by_name("Q101_VRAZKACHEST")->get_int());
        if(auto path=std::getenv("OPENGOTHIC_STASH_REPAIR_FILE")) {
          std::ifstream input(path);
          std::string target, item;
          size_t count=0;
          while(input >> std::quoted(target) >> item >> count) {
            Interactive* found=nullptr;
            for(uint32_t i=0;auto mob=w.mobsiById(i);++i)
              if(mob->isContainer() && (mob->tag()==target || mob->displayName()==target)) {
                if(found!=nullptr)
                  throw std::runtime_error("Ambiguous container repair target");
                found=mob;
                }
            if(found==nullptr || count==0 || vm.find_symbol_by_name(item)==nullptr)
              throw std::runtime_error("Invalid container repair entry");
            auto id=vm.find_symbol_by_name(item)->index();
            if(found->inventory().itemCount(id)!=0)
              throw std::runtime_error("Repair would duplicate an existing item");
            found->inventory().addItem(id,count,w);
            Log::i("[STASH_PROBE] restored target=",target," item=",item," count=",count);
            }
          if(!input.eof())
            throw std::runtime_error("Invalid container repair file");
          }

        for(uint32_t i=0;auto mob=w.mobsiById(i);++i) {
          auto p = mob->position();
          if(p.x<-35000 && p.x>-45000 && p.z<-150000 && p.z>-160000) {
            Log::i("[STASH_PROBE] mob=",mob->tag()," name=",mob->displayName()," y=",p.y);
            for(auto it=mob->inventory().iterator(Inventory::T_Inventory);it.isValid();++it)
              Log::i("[STASH_PROBE] item=",vm.find_symbol_by_index(uint32_t(it->clsId()))->name()," count=",it.count());
            }
          }
        auto fabio=w.findNpcByInstance(vm.find_symbol_by_name("NONE_5_FABIO")->index());
        if(fabio) {
          Log::i("[STASH_PROBE] Fabio gold=",fabio->inventory().goldCount());
          for(auto it=fabio->inventory().iterator(Inventory::T_Ransack);it.isValid();++it)
            Log::i("[STASH_PROBE] Fabio item=",vm.find_symbol_by_index(uint32_t(it->clsId()))->name()," count=",it.count());
          }
        pl.closeWeapon(true);
        auto wp=w.findPoint("SHIP_BEGINNING_AMULET",false);
        if(wp && std::getenv("OPENGOTHIC_STASH_KEEP_POSITION")==nullptr) {
          pl.setPosition(wp->position());
          pl.setDirection(wp->direction());
          Log::i("[STASH_PROBE] approached waypoint");
          }
        }
      if(std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr) {
        auto& w = *Gothic::inst().world();
        auto& pl = *Gothic::inst().player();
        const auto mode = std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"));
        if(mode=="ordinary" || mode=="ordinary-reload" || mode=="partial" || std::getenv("OPENGOTHIC_LOCK_FIXTURE")!=nullptr) {
          for(uint32_t i=0;auto mob=w.mobsiById(i);++i)
            if(mob->tag()=="Q101_CHEST_01") {
              lockProbeTarget=mob;
              break;
              }
          if(lockProbeTarget==nullptr)
            throw std::runtime_error("Ordinary lockpick probe target missing");
          const auto chestPos = lockProbeTarget->position();
          Log::i("[LOCK_PROBE] fixture prior_interaction=",pl.interactive()!=nullptr," synthetic=1");
          inventory.close();
          pl.setInteraction(nullptr,true);
          pl.clearAiQueue();
          pl.clearState(true);
          pl.clearGoTo();
          pl.closeWeapon(true);
          pl.setPosition(chestPos+Vec3(0,0,-150));
          pl.setDirection(chestPos-pl.position());
          pl.updateTransform();
          Gothic::inst().camera()->reset(&pl);
          if(mode=="cast" || mode=="spell-partial") {
            auto& vm = w.script().getVm();
            auto scroll = vm.find_symbol_by_name("ITSC_PICKLOCK")->index();
            if(pl.inventory().itemCount(scroll)==0)
              pl.addItem(scroll,1);
            pl.useItem(scroll,3,true);
            pl.handle().attribute[ATR_MANAMAX] = std::max(100,pl.attribute(ATR_MANAMAX));
            pl.handle().attribute[ATR_MANA] = 100;
            Log::i("[LOCK_PROBE] fixture=Q101_CHEST_01 item=ITSC_PICKLOCK mana=100 synthetic=1");
            }
          }
        if(mode=="ordinary" || mode=="ordinary-reload" || mode=="partial") {
          w.script().probeLockFocus(pl,*lockProbeTarget,mode=="ordinary-reload");
          if(mode!="ordinary-reload") {
            if(!lockProbeTarget->isLocked() || lockProbeTarget->pickLockCode().empty())
              throw std::runtime_error("Ordinary lockpick probe requires locked Q101_CHEST_01");
            const auto lockpick = w.script().lockPickId();
            while(pl.inventory().itemCount(lockpick)<2)
              pl.addItem(lockpick,1);
            }
          inventory.open(pl,*lockProbeTarget);
          if(mode!="ordinary-reload")
            Log::i("[LOCK_PROBE] ordinary target=",lockProbeTarget->tag()," code=",lockProbeTarget->pickLockCode(),
                   " lockpicks=",pl.inventory().itemCount(w.script().lockPickId())," state=",int(inventory.isOpen()));
          else
            Log::i("[LOCK_PROBE] ordinary reload cracked=",lockProbeTarget->isCracked()," chest_ui=",int(inventory.isOpen()));
          }
        else {
        auto active = pl.inventory().activeWeapon();
        Log::i("[LOCK_PROBE] loaded weapon=",int(pl.weaponState())," spell=",active ? active->spellId() : -1);
        pl.closeWeapon(true);
        auto plain = w.findFocus(Focus());
        Log::i("[LOCK_PROBE] unarmed focus=",plain.displayName()," mob=",plain.interactive ? plain.interactive->tag() : "none");
        lockProbeTarget = plain.interactive;
        if(plain.interactive)
          Log::i("[LOCK_PROBE] code=",plain.interactive->pickLockCode()," cracked=",plain.interactive->isCracked());
        if(std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="reload") {
          if(lockProbeTarget)
            inventory.open(pl,*lockProbeTarget);
          Log::i("[LOCK_PROBE] reload cracked=",lockProbeTarget && lockProbeTarget->isCracked()," chest_ui=",int(inventory.isOpen()));
          inventory.close();
          } else {
          auto spell = w.script().getVm().find_symbol_by_name("SPL_PICKLOCK");
          Log::i("[LOCK_PROBE] draw=",pl.drawSpell(spell->get_int()));
          }
        }
        }
      if(std::getenv("OPENGOTHIC_DIALOG_PROBE")!=nullptr && std::getenv("OPENGOTHIC_DIALOG_XP")!=nullptr) {
        auto& vm = Gothic::inst().world()->script().getVm();
        const int before = Gothic::inst().player()->handle().exp;
        Log::i("[DIALOG_PROBE] begin XP award");
        vm.call_function("B_GIVEPLAYERXP",50);
        Log::i("[DIALOG_PROBE] XP delta=",Gothic::inst().player()->handle().exp-before);
        }
      if(std::getenv("OPENGOTHIC_GATE_PROBE")!=nullptr)
        Gothic::inst().world()->execTriggerEvent(TriggerEvent("SHIP_TRAPDOOR", "ARCHOLOS_GATE_PROBE", TriggerEvent::T_Trigger));
      if(std::getenv("OPENGOTHIC_GATE_PROBE")!=nullptr)
        player.onKeyPressed(KeyCodec::Forward,Event::K_W,KeyCodec::Mapping(0));
      if(std::getenv("OPENGOTHIC_DIALOG_PROBE")!=nullptr) {
        auto& script = Gothic::inst().world()->script();
        auto& vm = script.getVm();
        auto info = std::static_pointer_cast<zenkit::IInfo>(vm.find_symbol_by_name("DIA_WILLEM_EXIT")->get_instance());
        auto npc = Gothic::inst().world()->findNpcByInstance(size_t(info->npc));
        auto pl = Gothic::inst().player();
        Log::i("[DIALOG_PROBE] npc=",npc!=nullptr," info=",info->information);
        if(npc!=nullptr) {
          GameScript::DlgChoice exit;
          exit.handle = info.get();
          exit.scriptFn = uint32_t(info->information);
          if(std::string_view(std::getenv("OPENGOTHIC_DIALOG_PROBE"))=="ui") {
            npc->clearState(true);
            npc->clearAiQueue();
            npc->setPosition(pl->position()+Tempest::Vec3(200,0,0));
            dialogProbeNpc = npc;
            } else {
            Log::i("[DIALOG_PROBE] begin exit");
            script.exec(exit,*pl,*npc);
            Log::i("[DIALOG_PROBE] returned exit");
            }
          }
        }
      if(auto mode = std::getenv("OPENGOTHIC_UI_PROBE")) {
        if(std::string_view(mode)=="journal")
          rootMenu.setMenu("MENU_LOG");
        Log::i("[MODAL_PROBE] in-game menu=",Gothic::inst().menuMain());
        setenv("OPENGOTHIC_UI_READY","1",1);
        }
      if(auto mode=std::getenv("OPENGOTHIC_BOSS_UI_PROBE")) {
        auto& w = *Gothic::inst().world();
        auto& vm = w.script().getVm();
        auto* pl = w.player();
        if(std::string_view(mode).starts_with("focus-")) {
          w.script().probeNpcFocus(*pl,std::string_view(mode)=="focus-reload");
          } else if(std::string_view(mode)=="seed") {
          vm.call_function("START_BOSSUI",pl->handlePtr(),1);
          Log::i("[BOSS_UI] synthetic start active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
                 " hp=",pl->attribute(ATR_HITPOINTS));
          } else if(std::string_view(mode)=="event" || std::string_view(mode)=="event-seed" || bossGeometry) {
          vm.call_function<std::string>("RAZORBOSSCOMMAND",std::string_view{});
          auto* razor = w.findNpcByInstance(vm.find_symbol_by_name("RAZOR_ARMORED")->index());
          if(razor==nullptr)
            throw std::runtime_error("SQ416 fixture did not insert RAZOR_ARMORED");
          auto* approach = w.findPoint("PART12_SQ416_CAVE_09",false);
          if(approach==nullptr)
            throw std::runtime_error("SQ416 fixture approach waypoint missing");
          pl->clearAiQueue();
          pl->clearState(true);
          pl->clearGoTo();
          pl->setPosition(approach->position());
          pl->setDirection(approach->direction());
          pl->updateTransform();
          if(bossGeometry) {
            auto* detlow = w.findNpcByInstance(vm.find_symbol_by_name("VLK_3015_DETLOW")->index());
            if(detlow==nullptr)
              throw std::runtime_error("SQ416 geometry fixture needs Detlow");
            detlow->clearAiQueue();
            detlow->clearState(true);
            detlow->clearGoTo();
            detlow->setPosition(pl->position()+Tempest::Vec3(-250,0,250));
            detlow->updateTransform();
            }
          if(auto camera=Gothic::inst().camera())
            camera->reset(pl);
          const auto heroPos = pl->position(), razorPos = razor->position();
          Log::i("[BOSS_UI] event positions hero=",heroPos.x,",",heroPos.y,",",heroPos.z,
                 " razor=",razorPos.x,",",razorPos.y,",",razorPos.z);
          vm.call_function("EVENTSMANAGER_SQ416");
          pl->handle().flags = zenkit::NpcFlag(uint32_t(pl->handle().flags) | uint32_t(zenkit::NpcFlag::IMMORTAL));
          Log::i("[BOSS_UI] event start active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
                 " state=",vm.find_symbol_by_name("SQ416_STARTBOSSFIGHT")->get_int()," hp=",razor->attribute(ATR_HITPOINTS));
          } else if(std::string_view(mode)=="event-reload") {
          auto* razor = w.findNpcByInstance(vm.find_symbol_by_name("RAZOR_ARMORED")->index());
          if(razor==nullptr)
            throw std::runtime_error("SQ416 restart lost RAZOR_ARMORED");
          Log::i("[BOSS_UI] event reload active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
                 " state=",vm.find_symbol_by_name("SQ416_STARTBOSSFIGHT")->get_int()," hp=",razor->attribute(ATR_HITPOINTS));
          } else {
          Log::i("[BOSS_UI] reload active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
                 " hp=",pl->attribute(ATR_HITPOINTS));
          }
        bossUiProbeStarted = true;
        }
      profileAt = profileEntry;
      Log::i("[ARCHOLOS_BEGIN] width=",swapchain.w()," height=",swapchain.h(),
             " scale=",Gothic::inst().settingsGetI("INTERNAL","vidResIndex"));

      }
    if(sampling && std::getenv("OPENGOTHIC_RECIPE_PROBE")!=nullptr) {
      if(profileFrames==0 || (profileFrames>=120 && recipeRereadFrame==0 &&
                              Gothic::inst().player()->bodyStateMasked()==BS_STAND)) {
        if(profileFrames!=0)
          recipeRereadFrame = profileFrames;
        auto& vm = Gothic::inst().world()->script().getVm();
        auto pl = Gothic::inst().player();
        const auto recipeId = vm.find_symbol_by_name("ITRE_RATSTICK")->index();
        auto doc = pl->getItem(recipeId);
        if(doc==nullptr)
          doc = pl->addItem(recipeId,1);
        auto talent = vm.find_symbol_by_name("PLAYER_TALENT_COOKING");
        const auto learnedIndex = uint16_t(vm.find_symbol_by_name("MEAL_RATSTICK")->get_int());
        if(profileFrames==0) {
          talent->set_int(0,learnedIndex);
          Log::i("[RECIPE_PROBE] begin learning hp=",doc->handle().hp);
          // Reproduce a callback entered with no current script item.
          vm.global_item()->set_instance(nullptr);
          } else {
          // An unrelated inventory query must not affect rereading the recipe.
          auto unrelated = vm.init_instance<zenkit::IItem>("ITMI_OLDCOIN");
          vm.global_item()->set_instance(unrelated);
          }
        auto previousItem = vm.global_item()->get_instance();
        auto previousSelf = vm.global_self()->get_instance();
        std::vector<int32_t> beforeTalents;
        for(uint16_t i=0;i<talent->count();++i)
          beforeTalents.push_back(talent->get_int(i));
        auto beforeLog = *Gothic::inst().questLog();
        pl->useItem(recipeId); // Same entry point as InventoryMenu::onItemAction.
        Log::i("[RECIPE_PROBE] learned=",talent->get_int(learnedIndex)," active=",document.isActive(),
               " context_restored=",vm.global_item()->get_instance()==previousItem && vm.global_self()->get_instance()==previousSelf);
        auto ql = Gothic::inst().questLog();
        bool preserved = true;
        for(uint16_t i=0;i<talent->count();++i)
          if(i!=learnedIndex && talent->get_int(i)!=beforeTalents[i])
            preserved = false;
        size_t entries = 0, afterEntries = 0;
        for(size_t i=0;i<beforeLog.questCount();++i) {
          auto& old = beforeLog.quest(i);
          auto& now = ql->quest(i);
          entries += old.entry.size();
          if(old.name!="Cooking" && (old.name!=now.name || old.entry!=now.entry || old.status!=now.status))
            preserved = false;
          }
        for(size_t i=0;i<ql->questCount();++i) {
          auto& q = ql->quest(i);
          afterEntries += q.entry.size();
          if(profileFrames==0) {
            Log::i("[RECIPE_PROBE] topic=",q.name);
            for(auto& e:q.entry)
              Log::i("[RECIPE_PROBE] entry=",e);
            }
          }
        // Exercise the actual cooking-choice condition with the stove's context.
        auto production = vm.find_symbol_by_name("PLAYER_MOBSI_PRODUCTION");
        auto mode = vm.find_symbol_by_name("COOKINGMEALS_MODE");
        const int oldProduction = production->get_int(), oldMode = mode->get_int();
        production->set_int(vm.find_symbol_by_name("MOBSI_COOKING")->get_int());
        mode->set_int(1);
        Log::i("[RECIPE_PROBE] stove_available=",vm.call_function<int>("PC_ITFO_RATSTICK_CONDITION"));
        production->set_int(oldProduction);
        mode->set_int(oldMode);
        Log::i("[RECIPE_PROBE] preserved=",preserved," reread_duplicate=",profileFrames!=0 && afterEntries!=entries);
        }
      if(profileFrames==30 || (recipeRereadFrame!=0 && profileFrames==recipeRereadFrame+30))
        Log::i("[RECIPE_PROBE] document_still_open=",document.isActive());
      if(profileFrames==60 || (recipeRereadFrame!=0 && profileFrames==recipeRereadFrame+40)) {
        document.close();
        Gothic::inst().player()->stopItemStateAnim();
        if(recipeRereadFrame!=0)
          Log::i("[RECIPE_PROBE] end");
        }
      }
    if(sampling && bossUiProbeStarted) {
      const auto mode = std::string_view(std::getenv("OPENGOTHIC_BOSS_UI_PROBE"));
      auto& w = *Gothic::inst().world();
      auto& vm = w.script().getVm();
      auto* pl = w.player();
      if(bossGeometry && bossUiGeometryFrame!=profileFrames) {
        bossUiGeometryFrame = profileFrames;
        const char* target = nullptr;
        if(profileFrames==0 || profileFrames==55 || profileFrames==115) target = "boss";
        if(profileFrames==15 || profileFrames==90 || profileFrames==240) target = "other";
        if(profileFrames==40 || profileFrames==105) target = "none";
        if(target) {
          setenv("OPENGOTHIC_BOSS_UI_FOCUS",target,1);
          Log::i("[BOSS_UI] geometry focus=",target," frame=",profileFrames);
          }
        if(profileFrames==70) setFullscreen(true);
        if(profileFrames==180) setFullscreen(false);
        if(profileFrames==120) {
          rootMenu.setMenu("MENU_LOG");
          rootMenu.setPlayer(*pl);
          Log::i("[BOSS_UI] geometry menu_open=",rootMenu.isActive()," tick=",w.tickCount());
          }
        if(profileFrames==150) {
          Log::i("[BOSS_UI] geometry menu_close tick=",w.tickCount());
          rootMenu.closeAll();
          }
        if(profileFrames==260) {
          const int before = pl->handle().exp;
          vm.call_function("B_GIVEPLAYERXP",50);
          Log::i("[BOSS_UI] XP delta=",pl->handle().exp-before);
          }
        if(std::getenv("OPENGOTHIC_BOSS_UI_CONSUMERS")) {
          if(profileFrames==280) {
            rootMenu.setMenu("MENU_STATUS");
            rootMenu.setPlayer(*pl);
            Log::i("[UI_CONSUMER] native_status bar=",vm.find_symbol_by_name("STATUSSCREEN_EXPBAR")->get_int());
            }
          if(profileFrames==290) {
            vm.find_symbol_by_name("STATUSSCREEN_EXPBAR_EXP")->set_int(50);
            vm.find_symbol_by_name("STATUSSCREEN_EXPBAR_NEXT")->set_int(100);
            vm.call_function("STATUSSCREEN_CREATEEXPBAR");
            Log::i("[UI_CONSUMER] explicit_status bar=",vm.find_symbol_by_name("STATUSSCREEN_EXPBAR")->get_int());
            }
          if(profileFrames==300) {
            vm.call_function("STATUSSCREENCLOSE_HOOK");
            rootMenu.closeAll();
            }
          if(profileFrames==310) {
            vm.call_function("CRAFTINGVIEW_SHOW",int32_t(vm.find_symbol_by_name("ITFO_BREAD")->index()),2,1);
            Log::i("[UI_CONSUMER] crafting open=",vm.find_symbol_by_name("CRAFTINGVIEW_ISOPEN")->get_int());
            }
          if(profileFrames==320)
            vm.call_function("CRAFTINGVIEW_HIDE");
          }
        }
      if(mode=="seed" && profileFrames==30) {
        pl->handle().attribute[ATR_HITPOINTS] /= 2;
        vm.call_function("BOSSUI_FF");
        Log::i("[BOSS_UI] synthetic health active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
               " hp=",pl->attribute(ATR_HITPOINTS));
        }
      if((mode=="seed" || mode=="focus-seed") && profileFrames==60 && !bossUiProbeSaved) {
        bossUiProbeSaved = true;
        saveGame("save_slot_2.sav","Boss UI synthetic fixture");
        Log::i("[BOSS_UI] synthetic save requested");
        }
      if((mode=="event" || mode=="event-seed" || bossGeometry) && profileFrames==30 && !bossUiProbeHealth) {
        bossUiProbeHealth = true;
        auto* razor = w.findNpcByInstance(vm.find_symbol_by_name("RAZOR_ARMORED")->index());
        razor->changeAttribute(ATR_HITPOINTS,-razor->attribute(ATR_HITPOINTS)/2,false);
        Log::i("[BOSS_UI] event health active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
               " hp=",razor->attribute(ATR_HITPOINTS));
        }
      if(mode=="event-seed" && profileFrames==45 && !bossUiProbeSaved) {
        bossUiProbeSaved = true;
        saveGame("save_slot_2.sav","Active SQ416 boss fixture");
        Log::i("[BOSS_UI] event save requested");
        }
      if((mode=="event" || mode=="event-reload" || bossGeometry) && profileFrames==(bossGeometry ? 220u : 60u) && !bossUiProbeFinished) {
        bossUiProbeFinished = true;
        auto* razor = w.findNpcByInstance(vm.find_symbol_by_name("RAZOR_ARMORED")->index());
        razor->changeAttribute(ATR_HITPOINTS,-razor->attribute(ATR_HITPOINTS),false);
        vm.call_function("EVENTSMANAGER_SQ416");
        Log::i("[BOSS_UI] event finish active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
               " state=",vm.find_symbol_by_name("SQ416_STARTBOSSFIGHT")->get_int()," dead=",razor->isDead());
        }
      if((mode=="event" || mode=="event-reload" || bossGeometry) && profileFrames==(bossGeometry ? 250u : 90u) && !bossUiProbeCleaned) {
        bossUiProbeCleaned = true;
        Log::i("[BOSS_UI] event cleanup active=",vm.find_symbol_by_name("BOSSUI")->get_int());
        }
      if(mode=="reload" && profileFrames==60) {
        vm.call_function("FINISH_BOSSUI");
        Log::i("[BOSS_UI] synthetic finish active=",vm.find_symbol_by_name("BOSSUI")->get_int());
        }
      if(mode.starts_with("legacy-") && profileFrames==60)
        Log::i("[LEGACY_UI] retained active=",vm.find_symbol_by_name("BOSSUI")->get_int(),
               " bar_valid=",vm.call_function<int>("HLP_ISVALIDHANDLE",vm.find_symbol_by_name("BOSS_BAR")->get_int()),
               " title_valid=",vm.call_function<int>("HLP_ISVALIDHANDLE",vm.find_symbol_by_name("BOSSNAMEPRINT")->get_int()));
      if(mode=="legacy-seed" && profileFrames==60) {
        Log::i("[LEGACY_UI] fixture timer_paused=",vm.find_symbol_by_name("_TIMER_PAUSED")->get_int());
        // Historical integration fixtures may intentionally retain paused timers.
        // Only the new private lifecycle needs a running timer; plain reload is untouched.
        vm.call_function("TIMER_SETPAUSE",0);
        vm.call_function("FINISH_BOSSUI");
        Log::i("[LEGACY_UI] cleanup active=",vm.find_symbol_by_name("BOSSUI")->get_int());
        vm.call_function("START_BOSSUI",pl->handlePtr(),0);
        Log::i("[LEGACY_UI] fresh active=",vm.find_symbol_by_name("BOSSUI")->get_int());
        Log::i("[LEGACY_UI] fresh bar_valid=",vm.call_function<int>("HLP_ISVALIDHANDLE",vm.find_symbol_by_name("BOSS_BAR")->get_int()),
               " destructor_arg=",vm.find_symbol_by_name("BAR_DELETE.BAR")->get_int());
        }
      if(mode=="legacy-seed" && profileFrames==90) {
        pl->handle().attribute[ATR_HITPOINTS] = pl->attribute(ATR_HITPOINTSMAX)/2;
        auto boss = vm.find_symbol_by_name("CURRENTBOSS")->get_instance();
        Log::i("[LEGACY_UI] health hero=",pl->attribute(ATR_HITPOINTS),
               " boss=",vm.find_symbol_by_name("C_NPC.ATTRIBUTE")->get_int(ATR_HITPOINTS,boss.get()),
               " max=",vm.find_symbol_by_name("C_NPC.ATTRIBUTE")->get_int(ATR_HITPOINTSMAX,boss.get()),
               " same=",boss==pl->handlePtr());
        Log::i("[LEGACY_UI] timer paused=",vm.find_symbol_by_name("_TIMER_PAUSED")->get_int(),
               " callback_active=",vm.call_function<int>("FF_ACTIVE",int32_t(vm.find_symbol_by_name("BOSSUI_FF")->index())));
        }
      if(mode=="legacy-seed" && profileFrames==120 && !bossUiProbeSaved) {
        bossUiProbeSaved = true;
        saveGame("save_slot_2.sav","Fresh UI after historical load");
        Log::i("[LEGACY_UI] fresh save requested");
        }
      }
    if(sampling && std::getenv("OPENGOTHIC_WORLD_PROBE")!=nullptr) {
      const auto mode = std::string_view(std::getenv("OPENGOTHIC_WORLD_PROBE"));
      auto& w = *Gothic::inst().world();
      auto& pl = *w.player();
      const auto sameWorld = [](std::string_view a, std::string_view b) {
        if(a.size()!=b.size()) return false;
        for(size_t i=0;i<a.size();++i)
          if(std::tolower(uint8_t(a[i]))!=std::tolower(uint8_t(b[i]))) return false;
        return true;
        };
      if(!worldProbeTransitioned && profileFrames==30) {
        const bool toSewers = mode=="to-sewers";
        const bool verifyMainland = mode=="verify-mainland";
        const char* expected = (toSewers || verifyMainland) ? "ARCHOLOS_MAINLAND.ZEN" : "ARCHOLOS_SEWERS.ZEN";
        if(!sameWorld(w.name(),expected))
          throw std::runtime_error("World-transition probe started in the wrong world");
        Interactive* lock = nullptr;
        for(uint32_t id=0;auto* mob=w.mobsiById(id);++id)
          if(((toSewers || verifyMainland) && mob->tag()=="Q101_CHEST_01") || (!toSewers && !verifyMainland && (mob->isContainer() || mob->isDoor()))) {
            lock=mob;
            break;
            }
        if(lock==nullptr)
          throw std::runtime_error("World-transition probe lock missing");
        if(verifyMainland) {
          if(lock->lockpickProgress()!=1)
            throw std::runtime_error("World-transition restart lost native lockpick progress");
          Log::i("[WORLD_PROBE] restart_lock_progress=1 world=",w.name());
          worldProbeTransitioned=true;
          }
        else {
        w.script().beginWorldTransitionProbe(pl,*lock);
        worldProbeTransitioned=true;
        const char* target = toSewers ? "ARCHOLOS_SEWERS.ZEN" : "ARCHOLOS_MAINLAND.ZEN";
        std::string trigger, start;
        if(!w.triggerChangeLevel(target,&trigger,&start))
          throw std::runtime_error("World-transition probe entrance trigger missing");
        Log::i("[WORLD_PROBE] zone_trigger=",trigger," target=",target," start=",start," synthetic=1");
          }
        }
      const char* destination = mode=="to-sewers" ? "ARCHOLOS_SEWERS.ZEN" : "ARCHOLOS_MAINLAND.ZEN";
      if(mode!="verify-mainland" && worldProbeTransitioned && !worldProbeSaved && sameWorld(w.name(),destination) && worldProbeWalkFrame==0) {
        worldProbeWalkFrame = profileFrames;
        worldProbeWalkStart = pl.position();
        player.onKeyPressed(KeyCodec::Forward,Event::K_W,KeyCodec::Mapping(0));
        }
      if(mode!="verify-mainland" && !worldProbeWalked && worldProbeWalkFrame!=0 && profileFrames>=worldProbeWalkFrame+60) {
        player.clearInput();
        const auto walked = (pl.position()-worldProbeWalkStart).length();
        if(walked<=50 || w.currentCs()!=nullptr || dialogs.isActive())
          throw std::runtime_error("World-transition destination is not controllable");
        auto shot = renderer.screenshoot(cmdId);
        device.readPixels(textureCast<const Texture2d&>(shot)).save("world-transition-destination.png");
        Log::i("[WORLD_PROBE] destination_control walked=",walked,
               " camera=",w.currentCs()!=nullptr," dialogue=",dialogs.isActive());
        worldProbeWalked = true;
        }
      if(mode!="verify-mainland" && worldProbeTransitioned && !worldProbeSaved && worldProbeWalked &&
         sameWorld(w.name(),destination) && profileFrames>=180) {
        Interactive* returnedLock = nullptr;
        if(mode=="to-mainland")
          for(uint32_t id=0;auto* mob=w.mobsiById(id);++id)
            if(mob->tag()=="Q101_CHEST_01") {
              returnedLock=mob;
              break;
              }
        if(mode=="to-mainland" && (returnedLock==nullptr || returnedLock->lockpickProgress()!=1))
          throw std::runtime_error("World-transition lost native lockpick progress");
        w.script().checkWorldTransitionProbe(pl,returnedLock);
        Log::i("[WORLD_PROBE] save world=",w.name());
        worldProbeSaved=true;
        saveGame("save_slot_2.sav","World transition compatibility test");
        }
      if(mode=="verify-mainland" && worldProbeTransitioned && !worldProbeSaved && profileFrames>=180) {
        w.script().verifyWorldTransitionProbe(pl);
        Log::i("[WORLD_PROBE] save world=",w.name());
        worldProbeSaved=true;
        saveGame("save_slot_2.sav","World transition compatibility test");
        }
      if(worldProbeSaved && Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
        Log::i("[WORLD_PROBE] save finalized");
        Tempest::SystemApi::exit();
        }
      }
    const bool lockProbeAction = sampling && lockProbeActionFrame!=profileFrames;
    if(std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && lockProbeAction)
      lockProbeActionFrame = profileFrames;
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="partial" && profileFrames==100) {
      const auto key = std::toupper(lockProbeTarget->pickLockCode().front())=='L' ? KeyCodec::Left : KeyCodec::Right;
      player.onKeyPressed(key,Event::K_NoKey,KeyCodec::Mapping(0));
      Log::i("[LOCK_PROBE] partial progress=",lockProbeTarget->lockpickProgress()," cracked=",lockProbeTarget->isCracked());
      }
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="ordinary" && profileFrames==90) {
      auto& pl = *Gothic::inst().player();
      const auto id = Gothic::inst().world()->script().lockPickId();
      const auto count = pl.inventory().itemCount(id);
      const auto dex = pl.attribute(ATR_DEXTERITY);
      pl.handle().attribute[ATR_DEXTERITY] = 100;
      const auto wrong = std::toupper(lockProbeTarget->pickLockCode().front())=='L' ? KeyCodec::Right : KeyCodec::Left;
      player.onKeyPressed(wrong,Event::K_NoKey,KeyCodec::Mapping(0));
      pl.handle().attribute[ATR_DEXTERITY] = dex;
      Log::i("[LOCK_PROBE] ordinary failed_without_break=",count==pl.inventory().itemCount(id));
      }
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr &&
       (std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="cast" ||
        std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="spell-partial") && profileFrames==100) {
      auto& w = *Gothic::inst().world();
      auto& pl = *Gothic::inst().player();
      auto f = w.findFocus(Focus());
      auto active = pl.inventory().activeWeapon();
      Log::i("[LOCK_PROBE] armed weapon=",int(pl.weaponState())," spell=",active ? active->spellId() : -1," focus=",f.displayName()," mob=",f.interactive ? f.interactive->tag() : "none");
      if(active) {
        lockProbeScroll = active->clsId();
        lockProbeCount = pl.inventory().itemCount(lockProbeScroll);
        }
      lockProbeMana = pl.attribute(ATR_MANA);
      auto& sc = w.script();
      const float facing = pl.rotation();
      pl.setDirection(facing+180);
      Log::i("[LOCK_PROBE] away_rejected=",sc.invokeMana(pl,nullptr,0)==SPL_SENDSTOP);
      pl.setDirection(facing);
      pl.changeAttribute(ATR_MANA,-lockProbeMana,false);
      Log::i("[LOCK_PROBE] no_mana_rejected=",sc.invokeMana(pl,nullptr,0)==SPL_SENDSTOP);
      pl.changeAttribute(ATR_MANA,lockProbeMana,false);
      if(lockProbeTarget) {
        lockProbeTarget->setAsCracked(true);
        Log::i("[LOCK_PROBE] unlocked_rejected=",sc.invokeMana(pl,nullptr,0)==SPL_SENDSTOP);
        lockProbeTarget->setAsCracked(false);
        }
      const auto origin = pl.position();
      const bool started = sc.invokeMana(pl,nullptr,0)==SPL_NEXTLEVEL;
      bool changedRejected = false;
      for(uint32_t id=0;auto* mob=w.mobsiById(id);++id) {
        if(mob==lockProbeTarget || !mob->isLocked() || mob->pickLockCode().empty())
          continue;
        pl.setPosition(mob->position()+Vec3(0,0,-150));
        pl.setDirection(mob->position()-pl.position());
        pl.updateTransform();
        if(w.findFocus(Focus()).interactive!=mob)
          continue;
        const auto progress = mob->lockpickProgress();
        changedRejected = started && sc.invokeMana(pl,nullptr,1)==SPL_SENDSTOP && mob->lockpickProgress()==progress;
        break;
        }
      pl.setPosition(origin);
      pl.setDirection(facing);
      pl.updateTransform();
      Log::i("[LOCK_PROBE] target_changed_rejected=",changedRejected);
      player.onKeyPressed(KeyCodec::ActionGeneric,Event::K_NoKey,KeyCodec::Mapping(0));
      player.onKeyPressed(KeyCodec::Forward,Event::K_W,KeyCodec::Mapping(0));
      Log::i("[LOCK_PROBE] cast input held");
      }
    if(sampling && !lockProbeSaved && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr &&
       std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="spell-partial" &&
       profileFrames>100 && lockProbeTarget->lockpickProgress()>0) {
      lockProbeSaved = true;
      auto& pl = *Gothic::inst().player();
      player.clearInput();
      pl.closeWeapon(true);
      pl.setInteraction(nullptr,true);
      Log::i("[LOCK_PROBE] spell_partial progress=",lockProbeTarget->lockpickProgress(),
             " cracked=",lockProbeTarget->isCracked()," scroll_used=",lockProbeCount-pl.inventory().itemCount(lockProbeScroll));
      saveGame("save_slot_2.sav","Open Lock partial test");
      }
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="ordinary" && profileFrames==100) {
      auto& w = *Gothic::inst().world();
      auto& pl = *Gothic::inst().player();
      const auto code = lockProbeTarget->pickLockCode();
      const char expected = char(std::toupper(code.front()));
      const auto wrong = expected=='L' ? KeyCodec::Right : KeyCodec::Left;
      lockProbeScroll = w.script().lockPickId();
      lockProbeCount = pl.inventory().itemCount(lockProbeScroll);
      lockProbeDexterity = pl.attribute(ATR_DEXTERITY);
      pl.handle().attribute[ATR_DEXTERITY] = -1;
      player.onKeyPressed(wrong,Event::K_NoKey,KeyCodec::Mapping(0));
      Log::i("[LOCK_PROBE] ordinary wrong key sent");
      }
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="ordinary" && profileFrames==101) {
      auto& pl = *Gothic::inst().player();
      const auto broken = lockProbeCount-pl.inventory().itemCount(lockProbeScroll);
      pl.handle().attribute[ATR_DEXTERITY] = lockProbeDexterity;
      for(char key:lockProbeTarget->pickLockCode())
        player.onKeyPressed(std::toupper(key)=='L' ? KeyCodec::Left : KeyCodec::Right,Event::K_NoKey,KeyCodec::Mapping(0));
      Log::i("[LOCK_PROBE] ordinary broken=",broken);
      }
    if(sampling && !lockProbeSaved && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr &&
       (std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE")).starts_with("ordinary") ||
        std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="partial") && profileFrames>=130) {
      lockProbeSaved = true;
      Log::i("[LOCK_PROBE] ordinary cracked=",lockProbeTarget->isCracked()," chest_ui=",int(inventory.isOpen()));
      inventory.close();
      Gothic::inst().player()->setInteraction(nullptr,true);
      saveGame("save_slot_2.sav","Ordinary lockpick test");
      }
    if(lockProbeAction && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr && std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="cast" && profileFrames==700) {
      auto& pl = *Gothic::inst().player();
      player.clearInput();
      Log::i("[LOCK_PROBE] complete cracked=",lockProbeTarget && lockProbeTarget->isCracked(),
             " scroll_used=",int(lockProbeCount)-int(pl.inventory().itemCount(lockProbeScroll)),
             " mana_used=",lockProbeMana-pl.attribute(ATR_MANA));
      pl.closeWeapon(true);
      if(lockProbeTarget)
        inventory.open(pl,*lockProbeTarget);
      }
    if(sampling && !lockProbeSaved && std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr &&
       ((std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="cast" && profileFrames>=890) ||
        (std::string_view(std::getenv("OPENGOTHIC_LOCK_PROBE"))=="reload" && profileFrames>=130))) {
      lockProbeSaved = true;
      Log::i("[LOCK_PROBE] chest_ui=",int(inventory.isOpen()));
      inventory.close();
      auto& pl = *Gothic::inst().player();
      pl.setInteraction(nullptr,true);
      saveGame("save_slot_2.sav","Open Lock test");
      }
    if(sampling && dialogProbeNpc!=nullptr) {
      if(dialogs.isNpcInDialog(dialogProbeNpc)) {
        dialogProbePending = false;
        if(dialogProbeRounds==0)
          Log::i("[DIALOG_PROBE] open round=",++dialogProbeRounds," npc_triggered=1");
        }
      if(!dialogs.isActive() && !dialogProbePending && dialogProbeRounds<2 && profileFrames>0 && profileFrames%120==0) {
        dialogProbeNpc->startDialog(*Gothic::inst().player());
        dialogProbePending = true;
        Log::i("[DIALOG_PROBE] open round=",++dialogProbeRounds);
        }
      }
    double profileStage = profileEntry;
    const auto profileStamp = [&](size_t stage) {
      if(!sampling)
        return;
      const double now = profileNow();
      profileMs[stage] += now-profileStage;
      profileStage = now;
      };
    static uint64_t time=Application::tickCount();

    if(T_UNLIKELY(Gothic::inst().isBenchmarkModeCi())) {
      const auto st = Gothic::inst().checkLoading();
      if(st==Gothic::LoadState::Loading) {
        // skip loading frames in benchmark-ci mode, for sake of easier tooling
        return;
        }
      }

    /*
      Note: game update goes first
      once player position is updated, animation bones(cameraBone in particular) can be updated
      lastly - camera position
      */
    const uint64_t dt = tick();
    // A script can end the session during tick; probes must not retain readiness.
    sampling = sampling && Gothic::inst().world()!=nullptr;
    profileStamp(0);
    updateAnimation(dt);
    profileStamp(1);
    tickCamera(dt);

    auto& sync = fence[cmdId];
    if(!sync.wait(0)) {
      // GPU rendering is not done, pass to next frame
      std::this_thread::yield();
      profileStamp(2);
      if(sampling)
        ++profileSkipped;
      return;
      }
    profileStamp(2);
    Resources::resetRecycled(cmdId);

    if(video.isActive()) {
      video.paint(device,cmdId);
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      }
    uiMesh [cmdId].update(device,uiLayer);
    numMesh[cmdId].update(device,numOverlay);
    profileStamp(3);

    const auto imageId = swapchain.currentImage();
    CommandBuffer& cmd = commands[cmdId];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(enc,cmdId,imageId,uiMesh[cmdId],numMesh[cmdId],inventory,video);
    }
    profileStamp(4);
    sync = device.submit(cmd);
    const char* bossCapture = nullptr;
    if(bossGeometry) {
      switch(profileFrames) {
        case 5: bossCapture="boss-full"; break;
        case 20: bossCapture="other"; break;
        case 45: bossCapture="none"; break;
        case 60: bossCapture="boss-half"; break;
        case 80: bossCapture="resized"; break;
        case 100: bossCapture="resized-other"; break;
        case 110: bossCapture="resized-none"; break;
        case 140: bossCapture="menu"; break;
        case 170: bossCapture="resumed"; break;
        case 210: bossCapture="window-restored"; break;
        case 255: bossCapture="cleanup"; break;
        case 270: bossCapture="xp"; break;
        case 285: bossCapture="status-native"; break;
        case 295: bossCapture="status-explicit"; break;
        case 315: bossCapture="crafting"; break;
        case 325: bossCapture="consumers-cleanup"; break;
        }
      }
    if(sampling && std::getenv("OPENGOTHIC_BOSS_UI_CAPTURE")!=nullptr &&
       (bossCapture || (!bossGeometry && (profileFrames==0 || profileFrames==35 || profileFrames==95)))) {
      sync.wait();
      const auto probe = std::getenv("OPENGOTHIC_BOSS_UI_PROBE");
      const auto mode = std::string_view(probe ? probe : "");
      const char* phase = bossCapture ? bossCapture : profileFrames==0 ? (mode=="event-reload" ? "restored" : "full") :
                          profileFrames==35 ? "half" : mode=="event-seed" ? "active" : "cleanup";
      auto file = string_frm("boss-ui-",phase,".png");
      auto capture = renderer.capture(cmdId,uiMesh[cmdId],numMesh[cmdId],inventory,video);
      device.readPixels(capture).save(file.c_str());
      Log::i("[BOSS_UI] capture=",phase," viewport=",w(),",",h()," focus_y=",Gothic::inst().world()->script().focusBarY(h()));
      }
    device.present(swapchain);
    profileStamp(5);
    cmdId = (cmdId+1u)%Resources::MaxFramesInFlight;

    auto t = Application::tickCount();
    if(t-time<16 && !Gothic::inst().isInGame() && !video.isActive()) {
      uint32_t delay = uint32_t(16-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    else if(maxFpsInv>0 && t-time<maxFpsInv) {
      uint32_t delay = uint32_t(maxFpsInv-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    fps.push(t-time);
    if(Gothic::inst().isBenchmarkMode() && Gothic::inst().world()!=nullptr && Gothic::inst().world()->currentCs()!=nullptr)
      benchmark.push(t-time);
    time = t;
    profileStamp(6);
    if(sampling && std::getenv("OPENGOTHIC_GATE_PROBE")!=nullptr && profileFrames%30==0) {
      const auto p=Gothic::inst().player()->position();
      Log::i("[GATE_INPUT] frame=",profileFrames," pos=",p.x,",",p.y,",",p.z," collision=",Gothic::inst().player()->hasCollision());
      }
    if(sampling && std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr && profileFrames==300) {
      auto& w=*Gothic::inst().world();
      auto& pl=*w.player();
      auto flag=w.script().getVm().find_symbol_by_name("Q101_VRAZKACHEST")->get_int();
      Log::i("[STASH_PROBE] end flag=",flag);
      for(uint32_t i=0;auto mob=w.mobsiById(i);++i)
        if(mob->displayName()=="Examine the boards") {
          Log::i("[STASH_PROBE] boards y=",mob->position().y);
          if(flag==2 && std::getenv("OPENGOTHIC_STASH_KEEP_POSITION")==nullptr) {
            auto pos=mob->position();
            pos.y=w.findPoint("SHIP_BEGINNING_AMULET",false)->position().y;
            pos.z+=100;
            pl.setPosition(pos);
            pl.setDirection(mob->position()-pos);
            }
          }
      }
    if(sampling && std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr && profileFrames==350 && std::getenv("OPENGOTHIC_STASH_KEEP_POSITION")==nullptr) {
      auto& w=*Gothic::inst().world();
      auto f=w.findFocus(Focus());
      Log::i("[STASH_PROBE] focus=",f.displayName());
      if(f.interactive && f.displayName()=="Examine the boards")
        inventory.open(*w.player(),*f.interactive);
      }
    if(sampling && std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr && profileFrames==650) {
      Log::i("[STASH_PROBE] chest_ui=",int(inventory.isOpen()));
      if(std::getenv("OPENGOTHIC_STASH_RETURN")!=nullptr && inventory.isOpen()==InventoryMenu::State::Chest) {
        auto& w=*Gothic::inst().world();
        auto& pl=*w.player();
        auto& vm=w.script().getVm();
        auto id=vm.find_symbol_by_name("ITMIS_Q101_VRAZKACHEST")->index();
        for(uint32_t i=0;auto mob=w.mobsiById(i);++i)
          if(mob->displayName()=="Examine the boards")
            pl.addItem(id,*mob,1);
        auto vrazka=w.findNpcByInstance(vm.find_symbol_by_name("NONE_14_VRAZKA")->index());
        Log::i("[STASH_PROBE] box_taken=",pl.inventory().itemCount(id));
        if(vrazka) {
          const auto broken=vm.find_symbol_by_name("ITMIS_Q101_VRAZKACHEST_BROKEN")->index();
          const auto before=w.hasItems("KM_VRAZKA",broken);
          w.script().invokeState(vrazka->handlePtr(),pl.handlePtr(),"DIA_VRAZKA_Q101_GOTCHEST_INFO");
          Log::i("[STASH_PROBE] followup_created=",w.hasItems("KM_VRAZKA",broken)-before);

          Log::i("[STASH_PROBE] box_after_return=",pl.inventory().itemCount(id));
          }
        }
      inventory.close();
      }
    if(sampling && std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr && profileFrames==750)
      saveGame("save_slot_2.sav","Archolos loot recovery");
    if(sampling && std::getenv("OPENGOTHIC_CAPTAIN_PROBE")!=nullptr && captainProbeSaved) {
      Log::i("[CAPTAIN_PROBE] save finalized");
      Tempest::SystemApi::exit();
      }
    if(sampling && std::getenv("OPENGOTHIC_CAPTAIN_PROBE")!=nullptr && captainFixtureSaved) {
      Log::i("[CAPTAIN_PROBE] fixture finalized");
      Tempest::SystemApi::exit();
      }
    if(sampling && std::getenv("OPENGOTHIC_CAPTAIN_PROBE")!=nullptr && !captainProbeSaved && profileFrames%300==0) {
      auto& w = *Gothic::inst().world();
      auto& vm = w.script().getVm();
      for(auto name : {"NONE_7_RUPERT","NONE_1_JORN","NONE_3_EZEKIEL"}) {
        auto n = w.findNpcByInstance(vm.find_symbol_by_name(name)->index());
        if(n!=nullptr) {
          auto p=n->position();
          Log::i("[CAPTAIN_PROBE] npc=",name," pos=",p.x,",",p.y,",",p.z," bs=",int(n->bodyStateMasked())," wp=",n->handle().wp);
          }
        }
      Log::i("[CAPTAIN_PROBE] camera_name=",w.currentCs()!=nullptr ? w.currentCs()->name() : "none");
      Log::i("[CAPTAIN_PROBE] frame=",profileFrames," camera=",w.currentCs()!=nullptr,
             " dialogue=",dialogs.isActive()," flag=",vm.find_symbol_by_name("Q101_CAPTAIN_CUTSCENEENABLE")->get_int(),
             " fade=",vm.find_symbol_by_name("FADESCREENSTATE")->get_int(),
             " alpha=",vm.find_symbol_by_name("FADESCREENCURRA")->get_int(),
             " tria=",vm.find_symbol_by_name("TRIA_RUNNING")->get_int(),
             " player_y=",w.player()->position().y);
      if(vm.find_symbol_by_name("Q101_CAPTAIN_CUTSCENEENABLE")->get_int()==11 &&
         vm.find_symbol_by_name("FADESCREENSTATE")->get_int()==0 &&
         w.currentCs()==nullptr && !Gothic::inst().camera()->isCutscene() && !dialogs.isActive()) {
        Log::i("[CAPTAIN_PROBE] complete");
        auto shot = renderer.screenshoot(cmdId);
        device.readPixels(textureCast<const Texture2d&>(shot)).save("captain-complete.png");
        captainProbeSaved=true;
        saveGame("save_slot_2.sav","Captain sequence test");
        }
      }
    if(sampling && std::getenv("OPENGOTHIC_FOREST_PROBE")!=nullptr) {
      auto& w = *Gothic::inst().world();
      auto& vm = w.script().getVm();
      if(forestProbeSaved) {
        Log::i("[FOREST_PROBE] save finalized");
        Tempest::SystemApi::exit();
        }
      if(!forestProbeSaved && profileFrames%300==0) {
        const int chosen=vm.find_symbol_by_name("Q102_JORNCHOSEN")->get_int();
        const int running=vm.find_symbol_by_name("TRIA_RUNNING")->get_int();
        Log::i("[FOREST_PROBE] frame=",profileFrames," camera=",w.currentCs()!=nullptr," dialogue=",dialogs.isActive(),
               " chosen=",chosen," tria=",running);
        if(profileFrames==300 || profileFrames==600) {
          auto shot = renderer.screenshoot(cmdId);
          device.readPixels(textureCast<const Texture2d&>(shot)).save(profileFrames==300 ? "forest-line-300.png" : "forest-line-600.png");
          }
        if(chosen && !running && !dialogs.isActive() && w.currentCs()==nullptr) {
          if(!w.player()->isPlayer())
            throw std::runtime_error("Forest dialogue did not restore player control");
          for(auto name : {"NONE_1_JORN","NONE_5_FABIO"}) {
            auto n = w.findNpcByInstance(vm.find_symbol_by_name(name)->index());
            if(&w.script().dialogSpeaker(*n)!=n)
              throw std::runtime_error("Trialogue speaker leaked after finish");
            }
          Log::i("[FOREST_PROBE] player control verified");
          Log::i("[FOREST_PROBE] speaker reset verified");
          Log::i("[FOREST_PROBE] complete");
          saveGame("save_slot_2.sav","Forest dialogue test");
          forestProbeSaved=true;
          }
        }
      }
    if(sampling) {
      if(auto mode=std::getenv("OPENGOTHIC_AI_WAIT_PROBE")) {
        auto& self = *Gothic::inst().world()->player();
        auto& w = *Gothic::inst().world();
        auto* target = w.findNpcByInstance(w.script().getVm().find_symbol_by_name("NONE_1_JORN")->index());
        auto* other = w.findNpcByInstance(w.script().getVm().find_symbol_by_name("NONE_5_FABIO")->index());
        if(std::string_view(mode)=="edges") {
          if(aiWaitProbeStep==0 && self.isAiQueueEmpty()) {
            Log::i("[AI_WAIT_PROBE] empty=1");
            Log::i("[AI_WAIT_PROBE] self=1");
            self.aiPush(AiQueue::aiWaitTillEnd(*target,aiWaitProbeTicket));
            aiWaitProbeStep=1;
            }
          if(aiWaitProbeStep==1 && self.isAiQueueEmpty()) {
            Log::i("[AI_WAIT_PROBE] completed=1");
            target->clearAiQueue();
            target->aiPush(AiQueue::aiWait(250));
            aiWaitProbeTicket=target->aiWaitTicket();
            self.aiPush(AiQueue::aiWaitTillEnd(*target,aiWaitProbeTicket));
            target->aiPush(AiQueue::aiWait(4000));
            aiWaitProbeStep=2;
            }
          if(aiWaitProbeStep==2 && self.isAiQueueEmpty() && target->isAiBusy()) {
            Log::i("[AI_WAIT_PROBE] snapshot=1");
            self.clearAiQueue();
            target->clearAiQueue();
            other->clearAiQueue();
            target->aiPush(AiQueue::aiWait(500));
            other->aiPush(AiQueue::aiWait(250));
            target->aiPush(AiQueue::aiWaitTillEnd(*other,other->aiWaitTicket()));
            other->aiPush(AiQueue::aiWaitTillEnd(*target,target->aiWaitTicket()));
            aiWaitProbeStep=3;
            }
          if(aiWaitProbeStep==3 && target->isAiQueueEmpty() && other->isAiQueueEmpty()) {
            Log::i("[AI_WAIT_PROBE] reciprocal=1");
            target->aiPush(AiQueue::aiWait(4000));
            self.aiPush(AiQueue::aiWaitTillEnd(*target,target->aiWaitTicket()));
            aiWaitProbeStep=4;
            }
          if(aiWaitProbeStep==4 && !self.isAiQueueEmpty() && target->isAiBusy()) {
            w.removeNpc(*target);
            aiWaitProbeStep=5;
            }
          if(aiWaitProbeStep==5 && !aiWaitProbeSaved && self.isAiQueueEmpty()) {
            Log::i("[AI_WAIT_PROBE] removed=1");
            Log::i("[AI_WAIT_PROBE] edges complete");
            aiWaitProbeSaved=true;
            saveGame("save_slot_2.sav","AI wait edge test");
            }
          if(profileFrames>=720 && !aiWaitProbeSaved)
            throw std::runtime_error("AI wait edge probe timed out");
          }
        if(std::string_view(mode)=="seed" && !aiWaitProbeSaved && profileFrames==30) {
          if(self.isAiQueueEmpty())
            throw std::runtime_error("AI wait persistence probe was not queued");
          Log::i("[AI_WAIT_PROBE] save pending=1");
          aiWaitProbeSaved=true;
          saveGame("save_slot_2.sav","AI wait persistence test");
          }
        if(std::string_view(mode)=="reload" && profileFrames==0) {
          if(self.isAiQueueEmpty())
            throw std::runtime_error("AI wait was lost on reload");
          Log::i("[AI_WAIT_PROBE] restored pending=1");
          }
        if(std::string_view(mode)=="reload" && profileFrames==120) {
          if(self.isAiQueueEmpty())
            throw std::runtime_error("AI wait did not block after reload");
          Log::i("[AI_WAIT_PROBE] still pending=1");
          }
        if(std::string_view(mode)=="reload" && !aiWaitProbeSaved && profileFrames==420) {
          if(!self.isAiQueueEmpty())
            throw std::runtime_error("AI wait did not finish after reload");
          Log::i("[AI_WAIT_PROBE] complete");
          aiWaitProbeSaved=true;
          saveGame("save_slot_2.sav","AI wait persistence test");
          }
        if(std::string_view(mode)=="legacy-nav-seed" && !aiWaitProbeSaved && profileFrames==30) {
          if(!target->isAiNavigationActive() || !target->isAiActionPending(aiWaitProbeTicket))
            throw std::runtime_error("AI wait legacy navigation was not active before save");
          Log::i("[AI_WAIT_PROBE] legacy navigation save pending=1");
          aiWaitProbeSaved=true;
          saveGame("save_slot_2.sav","AI wait legacy navigation test");
          }
        if(std::string_view(mode)=="legacy-nav-reload" && profileFrames==30) {
          if(self.isAiQueueEmpty() || !target->isAiNavigationActive() || !target->isAiActionPending(aiWaitProbeTicket))
            throw std::runtime_error("AI wait legacy navigation did not block after reload");
          Log::i("[AI_WAIT_PROBE] legacy navigation still pending=1");
          }
        if(std::string_view(mode)=="legacy-nav-reload" && profileFrames==0) {
          if(!target->isAiNavigationActive() || !target->isAiActionPending(aiWaitProbeTicket))
            throw std::runtime_error("AI wait legacy navigation was not active after reload");
          self.aiPush(AiQueue::aiWaitTillEnd(*target,aiWaitProbeTicket));
          }
        if(std::string_view(mode)=="legacy-nav-reload" && !aiWaitProbeSaved && profileFrames==420) {
          if(!self.isAiQueueEmpty())
            throw std::runtime_error("AI wait legacy navigation did not finish after reload");
          Log::i("[AI_WAIT_PROBE] legacy navigation complete");
          aiWaitProbeSaved=true;
          saveGame("save_slot_2.sav","AI wait legacy navigation test");
          }
        if(aiWaitProbeSaved && Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
          Log::i("[AI_WAIT_PROBE] save finalized");
          Tempest::SystemApi::exit();
          }
        }
      }
    if(sampling && std::getenv("OPENGOTHIC_CITY_PROBE")!=nullptr) {
      const auto musicProbe = std::getenv("OPENGOTHIC_MUSIC_PROBE");
      const bool musicFull = musicProbe!=nullptr && std::string_view(musicProbe)=="full";
      if(musicProbe!=nullptr && profileFrames%60==0)
        GameMusic::inst().traceFileMusic();
      auto& w = *Gothic::inst().world();
      auto* pl = w.player();
      if(std::getenv("OPENGOTHIC_KMLIB_PROBE")!=nullptr && profileFrames==180) {
        auto& vm = w.script().getVm();
        const auto gi = [&](const char* name) { return vm.find_symbol_by_name(name)->get_int(); };
        const auto si = [&](const char* name, int value) { vm.find_symbol_by_name(name)->set_int(value); };
        Log::i("[KMLIB_PROBE] natural=",vm.find_symbol_by_name("CURRENTMUSICZONE")->get_string());
        // This outer function dispatches the DLL wrappers through real script BL calls.
        vm.call_function("GAMESERVICES_INCREMENTSTATANDCHECKACHIEVEMENT",
                         std::string_view("STAT_ACHIEVEMENT_18"),2,std::string_view("ACHIEVEMENT_18"),5);
        Log::i("[KMLIB_PROBE] stat=",Gothic::settingsGetI("KMLIB_STATS","STAT_ACHIEVEMENT_18"),
               " unlocked=",Gothic::settingsGetI("KMLIB_ACHIEVEMENTS","ACHIEVEMENT_18"));
        const auto chapter=gi("KAPITEL"), noEntry=gi("NOLOCATIONENTRYCHECK");
        const auto haven=gi("B_LOCATIONENTRYCHECK.HAVENNOENTRY"), city=gi("B_LOCATIONENTRYCHECK.CITYNOENTRY");
        const auto circle=gi("SQ504_JOINEDWATERCIRCLE"), unequip=gi("WATERCIRCLE_UNEQUIP");
        const auto edx=gi("EDX");
        // Capture scene requests, leaving the installed location/quest logic intact.
        // Choreography is covered separately by captain/forest integration tests.
        auto scenes=std::make_shared<std::vector<int>>();
        vm.override_function("CUTSCENE_START",[scenes](int scene) { scenes->push_back(scene); });
        si("KAPITEL",2); si("NOLOCATIONENTRYCHECK",1);
        si("B_LOCATIONENTRYCHECK.HAVENNOENTRY",0); si("B_LOCATIONENTRYCHECK.CITYNOENTRY",0);
        Gothic::settingsSetI("SOUND","musicEnabled",0);
        w.script().setMusicZone("HAV",uint8_t(GameMusic::Day));
        Log::i("[KMLIB_PROBE] gated=",gi("B_LOCATIONENTRYCHECK.HAVENNOENTRY"));
        si("NOLOCATIONENTRYCHECK",0);
        w.script().setMusicZone("VIL",uint8_t(GameMusic::Day));
        w.script().setMusicZone("HAV",uint8_t(GameMusic::Day));
        Log::i("[KMLIB_PROBE] haven=",gi("B_LOCATIONENTRYCHECK.HAVENNOENTRY")," scenes=",scenes->size(),
               " theme=",vm.find_symbol_by_name("CURRENTMUSICZONE")->get_string()," edx_preserved=",gi("EDX")==edx);
        w.script().setMusicZone("HAV",uint8_t(GameMusic::Day));
        w.script().setMusicZone("HAV",uint8_t(GameMusic::Ngt|GameMusic::Fgt));
        Log::i("[KMLIB_PROBE] repeat_scenes=",scenes->size()," night=",vm.find_symbol_by_name("CURRENTMUSICZONE")->get_string());
        si("NOLOCATIONENTRYCHECK",1); si("SQ504_JOINEDWATERCIRCLE",1); si("WATERCIRCLE_UNEQUIP",1);
        w.script().setMusicZone("CIT",uint8_t(GameMusic::Day));
        const auto restricted=gi("WATERCIRCLE_UNEQUIP");
        w.script().setMusicZone("FOR",uint8_t(GameMusic::Day));
        Log::i("[KMLIB_PROBE] armor_restricted=",restricted," armor_released=",gi("WATERCIRCLE_UNEQUIP"));
        si("KAPITEL",chapter); si("NOLOCATIONENTRYCHECK",noEntry);
        si("B_LOCATIONENTRYCHECK.HAVENNOENTRY",haven); si("B_LOCATIONENTRYCHECK.CITYNOENTRY",city);
        si("SQ504_JOINEDWATERCIRCLE",circle); si("WATERCIRCLE_UNEQUIP",unequip);
        Gothic::settingsSetI("SOUND","musicEnabled",1);
        w.script().setMusicZone("CIT",uint8_t(GameMusic::Day));
        Gothic::inst().flushSettings();
        }
      if(musicFull && profileFrames>=180) {
        auto& vm = w.script().getVm();
        const auto overrideTrack = [&](const char* name) {
          vm.find_symbol_by_name("MUSIC_CURRENTOVERRIDE")->set_int(name==nullptr ? 0 : int32_t(vm.find_symbol_by_name(name)->index()));
          // Exercise BL dispatch through the real script wrapper, including its native override.
          vm.call_function("INIT_MUSICSYSTEM_ALWAYS");
          };
        const double delay = musicProbeStage==0 ? 0 : musicProbeStage==8 ? 38000 : 6500;
        if(musicProbeStage<13 && profileNow()-musicProbeAt>=delay) {
          Log::i("[MUSIC_PROBE] stage=",musicProbeStage);
          GameMusic::inst().traceFileMusic();
          switch(musicProbeStage) {
            case 0: overrideTrack(nullptr); break;
            case 1: {
              Gothic::settingsSetF("SOUND","musicVolume",0.2f);
              const auto started = profileNow();
              w.script().setMusicZone("VIL",uint8_t(GameMusic::Day));
              GameMusic::inst().tick(); // Start decoding, then cancel before completion.
              w.script().setMusicZone("CIT",uint8_t(GameMusic::Day));
              Log::i("[MUSIC_PROBE] cancelled load dispatch_ms=",profileNow()-started);
              break;
              }
            case 2: Gothic::settingsSetI("SOUND","musicEnabled",0); break;
            case 3:
              Gothic::settingsSetI("SOUND","musicEnabled",1);
              Gothic::settingsSetF("SOUND","musicVolume",0.5f);
              w.setDayTime(23,0);
              break;
            case 4: w.setDayTime(12,0); break;
            case 5:
              if(!w.script().setMusicZone("VIL",uint8_t(GameMusic::Day)))
                throw std::runtime_error("Missing village music zone");
              break;
            case 6: w.script().setMusicZone("VIL",uint8_t(GameMusic::Fgt)); break;
            case 7: overrideTrack("BATTLE_35"); break;
            case 8: overrideTrack(nullptr); break;
            case 9: w.script().setMusicZone("VIL",uint8_t(GameMusic::Ngt|GameMusic::Fgt)); break;
            case 10: w.script().setMusicZone("CIT",uint8_t(GameMusic::Day)); break;
            case 11: overrideTrack("BATTLE2_36"); break;
            case 12: break; // Save an active override; the reload run must restore it.
            }
          ++musicProbeStage;
          musicProbeAt=profileNow();
          }
        }
      if(cityProbeSaved) {
        Log::i("[CITY_PROBE] save finalized");
        if(std::getenv("OPENGOTHIC_KMLIB_MENU_PROBE")!=nullptr)
          Gothic::inst().gameSession()->exitSession();
        else
          Tempest::SystemApi::exit();
        }
      if(profileFrames==90) {
        cityWalkStart = pl->position();
        player.onKeyPressed(KeyCodec::Forward,Event::K_W,KeyCodec::Mapping(0));
        }
      if(profileFrames==150) {
        player.clearInput();
        Log::i("[CITY_PROBE] walked=",(pl->position()-cityWalkStart).length());
        }
      if(profileFrames==200)
        cityMeasuredAt = profileNow();
      if(std::getenv("OPENGOTHIC_PERSISTENCE_PROBE")!=nullptr && (profileFrames==60 || profileFrames==600))
        w.script().probePersistence(profileFrames==600);
      if(!cityProbeSaved && ((!musicFull && profileFrames==600) || (musicFull && musicProbeStage==13))) {
        size_t nearby = 0;
        w.detectNpc(pl->position(),2000,[&](Npc& npc) { if(&npc!=pl) ++nearby; });
        const auto pos = pl->position();
        Log::i("[CITY_PROBE] settled fps=",double(profileFrames-200)*1000.0/(profileNow()-cityMeasuredAt)," nearby=",nearby,
               " pos=",pos.x,",",pos.y,",",pos.z," camera=",w.currentCs()!=nullptr," dialogue=",dialogs.isActive());
        if(dialogs.isActive())
          for(uint32_t i=0;i<w.npcCount();++i) {
            auto* npc = w.npcById(i);
            if(npc!=nullptr && dialogs.isNpcInDialog(npc))
              Log::i("[CITY_PROBE] dialogue participant=",npc->displayName());
            }
        if(w.currentCs()!=nullptr || dialogs.isActive())
          throw std::runtime_error("City exploration is blocked by a scene");
        auto shot = renderer.screenshoot(cmdId);
        device.readPixels(textureCast<const Texture2d&>(shot)).save("city-exploration.png");
        cityProbeSaved=true;
        saveGame("save_slot_2.sav","CITY EXPLORATION - Chapter 2");
        }
      }
    if(sampling && std::getenv("OPENGOTHIC_BEACH_PROBE")!=nullptr) {
      auto& w = *Gothic::inst().world();
      auto& vm = w.script().getVm();
      const bool fresh=std::string_view(std::getenv("OPENGOTHIC_BEACH_PROBE"))=="fresh";
      if(beachProbeSaved) {
        Log::i("[BEACH_PROBE] save finalized");
        Tempest::SystemApi::exit();
        }
      if(!fresh) {
        if(profileFrames==30)
          player.onKeyPressed(KeyCodec::Weapon,Event::K_NoKey,KeyCodec::Mapping(0));
        if(profileFrames==31)
          player.clearInput();
        if(profileFrames==120)
          w.player()->closeWeapon(true);
        if(beachTorch==nullptr) {
          beachTorch=w.findItemByInstance(vm.find_symbol_by_name("ITLSTORCHBURNING")->index(),beachTorchCount);
          if(beachTorch!=nullptr) {
            beachTorchStartY=beachTorch->position().y;
            Log::i("[BEACH_PROBE] torch spawned dynamic=",beachTorch->isDynamic()," y=",beachTorchStartY);
            }
          }
        if(profileFrames%120==0) {
          auto* n = w.findNpcByInstance(vm.find_symbol_by_name("NONE_3_EZEKIEL")->index());
          auto* wp = n->currentWayPoint();
          auto* ta = n->currentTaPoint();
          auto p = n->position();
          Log::i("[BEACH_PROBE] frame=",profileFrames," ezekiel=",p.x,",",p.y,",",p.z,
                 " sitting=",(n->bodyStateMasked()&BS_MAX)==(BS_SIT&BS_MAX)," wp=",n->handle().wp,
                 " current=",wp ? wp->name : "none"," routine=",ta ? ta->name : "none");
          if(beachTorch) {
            auto p = beachTorch->position();
            auto ground = w.physic()->landRay(p+Tempest::Vec3(0,50,0));
            Log::i("[BEACH_PROBE] torch frame=",profileFrames," fall=",beachTorchStartY-p.y,
                   " ground_distance=",p.y-ground.v.y," ground_hit=",ground.hasCol," held=",w.player()->isUsingTorch());
            }
          }
        }
      if(profileFrames==(fresh ? 180u : 1200u)) {
        auto shot = renderer.screenshoot(cmdId);
        device.readPixels(textureCast<const Texture2d&>(shot)).save("beach-complete.png");
        if(fresh) {
          Log::i("[BEACH_PROBE] fresh loot complete");
          Tempest::SystemApi::exit();
          } else {
          beachProbeSaved=true;
          saveGame("save_slot_2.sav","Beach compatibility test");
          }
        }
      }
    if(sampling && ++profileFrames==(bossGeometry ? (std::getenv("OPENGOTHIC_BOSS_UI_CONSUMERS") ? 330u : 280u) : std::getenv("OPENGOTHIC_MUSIC_PROBE")!=nullptr ? 12000u : std::getenv("OPENGOTHIC_CITY_PROBE")!=nullptr ? 1200u : std::getenv("OPENGOTHIC_RECIPE_PROBE")!=nullptr ? (recipeRereadFrame==0 ? 9000u : recipeRereadFrame+60) : std::getenv("OPENGOTHIC_FOREST_PROBE")!=nullptr ? 12000u : (std::getenv("OPENGOTHIC_AI_WAIT_PROBE")!=nullptr ? 900u : (std::getenv("OPENGOTHIC_BEACH_PROBE")!=nullptr ? 1800u : (std::getenv("OPENGOTHIC_CAPTAIN_PROBE")!=nullptr ? 36000u : ((std::getenv("OPENGOTHIC_UI_PROBE")!=nullptr || std::getenv("OPENGOTHIC_LOCK_PROBE")!=nullptr || std::getenv("OPENGOTHIC_STASH_PROBE")!=nullptr || std::getenv("OPENGOTHIC_WORLD_PROBE")!=nullptr) ? 900u : (dialogProbeNpc!=nullptr ? 2400u : (std::getenv("OPENGOTHIC_GATE_PROBE")!=nullptr ? 600u : 180u)))))))) {
      const double ms = (profileNow()-profileAt)/double(profileFrames);
      Log::i("[ARCHOLOS_PROFILE] frames=",profileFrames," skipped=",profileSkipped,
             " frame_ms=",ms," fps=",1000.0/ms,
             " tick_ms=",profileMs[0]/profileFrames,
             " animation_ms=",profileMs[1]/profileFrames,
             " camera_fence_ms=",profileMs[2]/profileFrames,
             " ui_ms=",profileMs[3]/profileFrames,
             " encode_ms=",profileMs[4]/profileFrames,
             " submit_present_ms=",profileMs[5]/profileFrames,
             " limiter_ms=",profileMs[6]/profileFrames);
      if(dialogProbeNpc!=nullptr)
        Log::i("[DIALOG_PROBE] finished rounds=",dialogProbeRounds," active=",dialogs.isActive());
      auto profileImage = renderer.screenshoot(cmdId);
      device.readPixels(textureCast<const Texture2d&>(profileImage)).save("profile.png");
      Tempest::SystemApi::exit();
      }
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    Log::e("swapchain is outdated - reset renderer");
    device.waitIdle();
    swapchain.reset();
    renderer.resetSwapchain();
    }
  }

double MainWindow::Fps::get() const {
  uint64_t sum=0,num=0;
  for(auto& i:dt)
    if(i>0) {
      sum+=i;
      num++;
      }
  if(num==0 || sum==0)
    return 60;
  uint64_t fps = (1000*100*num)/sum;
  return double(fps)/100.0;
  }

void MainWindow::Fps::push(uint64_t t) {
  for(size_t i=9;i>0;--i)
    dt[i]=dt[i-1];
  dt[0]=t;
  }

void MainWindow::BenchmarkData::push(uint64_t t) {
  fpsSum += t>0 ? (1000.0/double((t))) : 60.0;
  numFrames++;
  auto at = std::lower_bound(low1procent.begin(), low1procent.end(), t, std::greater<uint64_t>());
  low1procent.insert(at, t);
  low1procent.resize(std::min(low1procent.size(), (numFrames+99)/100));
  }

void MainWindow::BenchmarkData::clear() {
  low1procent.reserve(128);
  low1procent.clear();
  numFrames = 0;
  fpsSum = 0;
  }
