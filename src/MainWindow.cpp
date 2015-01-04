#include "MainWindow.h"

#include <QApplication>
#ifndef Q_OS_MAC
#include <QGridLayout>
#endif
#include <QStandardPaths>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>

#include "MapFileIO.h"
#include "OsmLayer.h"

MainWindow::MainWindow(QWidget * parent,
                       Qt::WindowFlags flags) :
    QMainWindow(parent, flags),
    _fileIO(new MapFileIO(this)),
    _glWidget(new GLWidget()),
    _playbackWidget(new PlaybackWidget()),
    _layerListWidget(new LayerListWidget()),
    _trackFileReader(new TrackFileReader(this)),
    _numPendingLayers(0),
    _aboutDialog(new AboutDialog(this)),
    _settings(new Settings(this))
{
    _networkAccessManager = Singleton<QNetworkAccessManager>::Instance();
    _networkAccessManager->setParent(this);
    _diskCache = new QNetworkDiskCache(this);
    QString cacheDir = getNetworkCacheDir();
    _diskCache->setCacheDirectory(cacheDir);
    _networkAccessManager->setCache(_diskCache);
    
    _preferencesDialog = new PreferencesDialog(_settings, this);
    
    /* export images dialog */
    _exportImageDialog = new ExportImageDialog(_glWidget, _playbackWidget, this);
    
    /* The trackFileReader reads tracks in the background */
    connect(_trackFileReader, &TrackFileReader::signalReady,
            this, &MainWindow::slotTrackFileLoaded);
    connect(_trackFileReader, &TrackFileReader::signalError,
            this, &MainWindow::slotTrackFileLoadError);
    connect(_trackFileReader, &TrackFileReader::signalUpdate,
            _playbackWidget, &PlaybackWidget::updateStatus);
    connect(_trackFileReader, &TrackFileReader::signalDone,
            _playbackWidget, &PlaybackWidget::hideProgress);

	QWidget *myCentralWidget = new QWidget();
    setCentralWidget(myCentralWidget);
#ifdef Q_OS_MAC
	// Placeholder hidden centralWidget to keep Qt happy
	_menuBar = new QMenuBar(0));
#else
	// On non-Mac platforms the main window == the GLWidget
	// We even set our object name to "GLWidget"
	QGridLayout *layout = new QGridLayout(centralWidget());
	_glWidget->setParent(centralWidget());
	layout->setMargin(0);
	layout->addWidget(_glWidget);
	setObjectName("GLWidget");
	_menuBar = menuBar();
#endif

    /* file menu */
    QMenu *_fileMenu = _menuBar->addMenu("File");
    _newMapAction = new QAction("&New Map", this);
    _addLayerAction = new QAction("&Add Track File...", this);
    _openMapFileAction = new QAction("&Open Map...", this);
    _saveAsMapFileAction = new QAction("Save Map As...", this);
    _saveMapFileAction = new QAction("&Save Map", this);
    _fileMenu->addAction(_newMapAction);
    _fileMenu->addSeparator();
    _fileMenu->addAction(_addLayerAction);
    _recentLayersMenu = new QMenu("Add recent track file");
    _clearRecentLayersMenuAction = new QAction("Clear menu", this);
    connect(_clearRecentLayersMenuAction, &QAction::triggered,
            _recentLayersMenu, &QMenu::clear);
    _fileMenu->addMenu(_recentLayersMenu);
    connect(_recentLayersMenu, &QMenu::triggered,
            this, &MainWindow::slotAddRecentLayer);
    _fileMenu->addSeparator();
    _fileMenu->addAction(_openMapFileAction);
    _recentMapsMenu = new QMenu("Open recent map");
    _clearRecentMapsMenuAction = new QAction("Clear menu", this);
    connect(_clearRecentMapsMenuAction, &QAction::triggered,
             _recentMapsMenu, &QMenu::clear);
    _fileMenu->addMenu(_recentMapsMenu);
    connect(_recentMapsMenu, &QMenu::triggered,
            this, &MainWindow::slotOpenRecentMapFile);
    _fileMenu->addSeparator();
    _fileMenu->addAction(_saveMapFileAction);
    _fileMenu->addAction(_saveAsMapFileAction);
    _showExportImagesDialogAction = new QAction("Export images...", this);
    _fileMenu->addAction(_showExportImagesDialogAction);
    connect(_newMapAction, SIGNAL(triggered(bool)),
            this, SLOT(slotNewMap()));
    connect(_openMapFileAction, SIGNAL(triggered(bool)),
            this, SLOT(slotOpenMapFile()));
    connect(_saveMapFileAction, SIGNAL(triggered(bool)),
            this, SLOT(slotSaveMapFile()));
    connect(_saveAsMapFileAction, SIGNAL(triggered(bool)),
            this, SLOT(slotSaveMapFileAs()));
    connect(_addLayerAction, SIGNAL(triggered(bool)),
            this, SLOT(slotAddLayer()));
    connect(_showExportImagesDialogAction, SIGNAL(triggered(bool)),
            _exportImageDialog, SLOT(show()));
    
    /* View menu */
    QMenu *_viewMenu = _menuBar->addMenu("View");
    _frameSelectedLayersAction = new QAction("Frame selected layers", this);
    _lockViewToLayerAction = new QAction("Lock view to selected layer", this);
    _setStartingViewAreaAction = new QAction("Set starting view area", this);
    _viewMenu->addAction(_frameSelectedLayersAction);
    _viewMenu->addAction(_lockViewToLayerAction);
    _viewMenu->addSeparator();
    _viewMenu->addAction(_setStartingViewAreaAction);
    connect(_lockViewToLayerAction, &QAction::triggered,
            this, &MainWindow::slotLockViewToSelectedLayer);
    connect(_frameSelectedLayersAction, &QAction::triggered,
            this, &MainWindow::slotFrameSelectedLayers);
    connect(_setStartingViewAreaAction, &QAction::triggered,
            this, &MainWindow::slotSetStartingViewArea);
    
    /* Window menu */
    QMenu *_windowMenu = _menuBar->addMenu("Window");
    _playCtrlWindowAction = new QAction("Playback controls", this);
    _layerListWindowAction = new QAction("Layer list", this);
    _mapWindowAction = new QAction("Map", this);
    _windowMenu->addAction(_playCtrlWindowAction);
    _windowMenu->addAction(_layerListWindowAction);
    _windowMenu->addAction(_mapWindowAction);
    connect(_playCtrlWindowAction, SIGNAL(triggered()),
            SLOT(slotShowPlaybackWidget()));
    connect(_layerListWindowAction, SIGNAL(triggered()),
            SLOT(slotShowLayerListWidget()));
    connect(_mapWindowAction, SIGNAL(triggered()), SLOT(slotShowMapWindow()));
    
    /* Playback controls */
    _forwardAction = new QAction("Play", this);
    _backAction = new QAction("Reverse", this);
    _rewindAction = new QAction("Rewind", this);
    _pauseAction = new QAction("Pause", this);
    
    connect(_playbackWidget, SIGNAL(signalForward()),
            _forwardAction, SLOT(trigger()));
    connect(_playbackWidget, SIGNAL(signalBack()),
            _backAction, SLOT(trigger()));
    connect(_playbackWidget, SIGNAL(signalPause()),
            _pauseAction, SLOT(trigger()));
    connect(_playbackWidget, SIGNAL(signalRewind()),
            _rewindAction, SLOT(trigger()));
    connect(_playbackWidget, SIGNAL(signalPlaybackRateChanged(double)),
            this, SLOT(slotPlaybackRateChanged(double)));
    
    connect(_forwardAction, SIGNAL(triggered()),
            _glWidget, SLOT(slotPlay()));
    connect(_backAction, SIGNAL(triggered()),
            _glWidget, SLOT(slotReverse()));
    connect(_pauseAction, SIGNAL(triggered()),
            _glWidget, SLOT(slotPause()));
    connect(_rewindAction, SIGNAL(triggered()),
            _glWidget, SLOT(slotRewind()));
    
    connect(_playbackWidget, SIGNAL(signalTimeSliderChanged(int)),
            SLOT(onTimeSliderDrag(int)));
    
    connect(_glWidget, SIGNAL(signalTimeChanged(double)),
            this, SLOT(slotTimeChanged(double)));
    connect(_glWidget->getMap(), SIGNAL(signalLayerAdded(LayerId)),
            this, SLOT(slotLayerAdded(LayerId)));
    connect(_glWidget, SIGNAL(signalLayersSelected(QList<LayerId>)),
            _layerListWidget, SLOT(slotSetSelectedLayers(QList<LayerId>)));
    
    /* About menu */
    _showAboutDialogAction = new QAction("About RunParticles", this);
    _showAboutDialogAction->setMenuRole(QAction::AboutRole);
    connect(_showAboutDialogAction, &QAction::triggered,
            this, &MainWindow::slotShowAboutDialog);
    QMenu *aboutMenu = _menuBar->addMenu("About...");
    aboutMenu->addAction(_showAboutDialogAction);
    
    /* Preferences menu item */
    _showPreferencesDialogAction = new QAction("Preferences...", this);
    _showPreferencesDialogAction->setMenuRole(QAction::PreferencesRole);
    connect(_showPreferencesDialogAction, &QAction::triggered,
            this, &MainWindow::slotShowPreferencesDialog);
    QMenu *prefsMenu = _menuBar->addMenu("Preferences...");
    prefsMenu->addAction(_showPreferencesDialogAction);
    
    // Application keyboard shortcuts
    _setupShortcuts();
    
    // Connect the layer list signals
    connect(_layerListWidget, SIGNAL(signalFrameLayers(QList<LayerId>)),
            this, SLOT(slotFrameLayers(const QList<LayerId>)));
    connect(_layerListWidget,
            SIGNAL(signalLayerSelectionChanged(QList<LayerId>)),
            this, SLOT(slotLayerSelectionChanged(const QList<LayerId>)));
    connect(_layerListWidget,
            SIGNAL(signalLayerVisibilityChanged(LayerId, bool)),
            this, SLOT(slotLayerVisibilityChanged(LayerId, bool)));
    connect(_layerListWidget,
            SIGNAL(signalLockViewToLayer(LayerId)),
            _glWidget, SLOT(slotLockViewToLayer(LayerId)));
    
    connect(qApp, &QCoreApplication::aboutToQuit,
            this, &MainWindow::slotAboutToQuit);
    
    slotTimeChanged(0);
    restoreSettings();
    _glWidget->frameLonLatBox(_settings->getStartingViewArea());
    _layerListWidget->show();
    _playbackWidget->show();
#ifdef Q_OS_MAC
    _glWidget->show();
#else
	// we must be visible before adding layers
    show();
#endif
    _loadBaseMap();
}

MainWindow::~MainWindow()
{
    // empty
}

#ifndef Q_OS_MAC
void
MainWindow::closeEvent(QCloseEvent *event)
{
	event->accept();
	qApp->quit();
}
#endif

void
MainWindow::_setupShortcuts()
{
    // Application-wide shortcuts
    // toggle play
    _playPauseShortcut = new QShortcut(QKeySequence(Qt::Key_Space),
                                       _playbackWidget);
    _playPauseShortcut->setContext(Qt::ApplicationShortcut);
    connect(_playPauseShortcut, SIGNAL(activated()),
            _glWidget, SLOT(slotTogglePlayPause()));
    // play
    _playShortcut = new QShortcut(QKeySequence(Qt::Key_L), _playbackWidget);
    _playShortcut->setContext(Qt::ApplicationShortcut);
    connect(_playShortcut, SIGNAL(activated()),
            _glWidget, SLOT(slotPlay()));
    // play in reverse
    _playReverseShortcut = new QShortcut(QKeySequence(Qt::Key_J),
                                         _playbackWidget);
    _playReverseShortcut->setContext(Qt::ApplicationShortcut);
    connect(_playReverseShortcut, SIGNAL(activated()),
            _glWidget, SLOT(slotReverse()));
    // pause
    _pauseShortcut = new QShortcut(QKeySequence(Qt::Key_K),
                                   _playbackWidget);
    _pauseShortcut->setContext(Qt::ApplicationShortcut);
    connect(_pauseShortcut, SIGNAL(activated()),
            _glWidget, SLOT(slotPause()));
    // add layer
    _addLayerShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_A),
                                      _playbackWidget);
    _addLayerShortcut->setContext(Qt::ApplicationShortcut);
    connect(_addLayerShortcut, SIGNAL(activated()),
            _addLayerAction, SLOT(trigger()));
    // save map
    _saveMapShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S),
                                     _playbackWidget);
    _saveMapShortcut->setContext(Qt::ApplicationShortcut);
    connect(_saveMapShortcut, SIGNAL(activated()),
            _saveMapFileAction, SLOT(trigger()));
    // open map
    _openMapShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_O),
                                     _playbackWidget);
    _openMapShortcut->setContext(Qt::ApplicationShortcut);
    connect(_openMapShortcut, SIGNAL(activated()),
            _openMapFileAction, SLOT(trigger()));
    // new map
    _newMapShortcut = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_N),
                                    _playbackWidget);
    _newMapShortcut->setContext(Qt::ApplicationShortcut);
    connect(_newMapShortcut, SIGNAL(activated()),
            _newMapAction, SLOT(trigger()));
}

void
MainWindow::loadTrackFile(const QString &trackFilePath)
{
    QList<Track*> *tracks = new QList<Track*>();
    _trackFileReader->readDeferred(trackFilePath, tracks);
}

bool
MainWindow::loadMapFile(const QString &path)
{
    _fileIO->setFilename(path);
    _fileIO->loadMapFile();
    QString trackFile;
    foreach(trackFile, _fileIO->getTrackFiles()) {
        loadTrackFile(trackFile);
    }
    LonLatBox mapViewArea = _fileIO->getViewArea();
    if (mapViewArea.valid()) {
        _glWidget->frameLonLatBox(mapViewArea);
    }
    return true;
}

void
MainWindow::clearMap()
{
    _fileIO->clear();
    _glWidget->getMap()->clearLayers();
    _layerListWidget->clear();
    _loadBaseMap();
    _playbackWidget->setTimeSliderMaximum(1800); /* 30 minutes */
    _glWidget->update();
}

bool
MainWindow::saveMapFile(const QString &path)
{
    _fileIO->setFilename(path);
    return _fileIO->writeMapFile(_settings->getSaveRelativePaths());
}

bool
MainWindow::confirmAbandonMap()
{
    if (_fileIO->isDirty()) {
        QMessageBox::StandardButton res = QMessageBox::question(this,
            "Save changes to map?",
            "Map has unsaved changes, save it or discard?",
            QMessageBox::Save | QMessageBox::Cancel | QMessageBox::Discard,
            QMessageBox::Save);
        if (res == QMessageBox::Cancel)
            return false;
        else if (res == QMessageBox::Save)
            return slotSaveMapFile();
    }
    return true;
}

QString
MainWindow::getNetworkCacheDir() const
{
    QString cacheLoc = QStandardPaths::writableLocation
                                                (QStandardPaths::CacheLocation);
    if (cacheLoc.isEmpty()) {
        cacheLoc = QDir::homePath()+"/."+QCoreApplication::applicationName();
    }
    return cacheLoc;
}

void
MainWindow::saveSettings()
{
#ifdef Q_OS_MAC
    _settings->saveWidgetState(_glWidget);
#else
	_settings->saveWidgetState(this);
#endif
    _settings->saveWidgetState(_playbackWidget);
    _settings->saveWidgetState(_layerListWidget);
    QAction *recentAction;
    QList<QString> recents;
    foreach(recentAction, _recentLayersMenu->actions()) {
        recents.append(recentAction->data().toString());
    }
    _settings->setRecentLayerFiles(recents);
    recents.clear();
    foreach(recentAction, _recentMapsMenu->actions()) {
        recents.append(recentAction->data().toString());
    }
    _settings->setRecentMapFiles(recents);
}

void
MainWindow::restoreSettings()
{
#ifdef Q_OS_MAC
    _settings->restoreWidgetState(_glWidget);
#else
	_settings->restoreWidgetState(this);
#endif
    _settings->restoreWidgetState(_playbackWidget);
    _settings->restoreWidgetState(_layerListWidget);

    QString path;
    foreach(path, _settings->getRecentMapFiles()) {
        _addPathToRecentMenu(_recentMapsMenu, path);
    }
    foreach(path, _settings->getRecentLayerFiles()) {
        _addPathToRecentMenu(_recentLayersMenu, path);
    }
}

void
MainWindow::applyTrackStyleRule(const TrackStyleRules &rules,TrackLayer *layer)
{
    TrackStyleRule rule = rules.getStyleForTrackType(layer->sport());
    layer->setTrackColor(Color(rule.color.redF(),
                               rule.color.greenF(),
                               rule.color.blueF()));
    layer->setTrackWidth(rule.width);
}

void
MainWindow::applyTrackStyleRules(const TrackStyleRules &rules)
{
    LayerPtr layer;
    foreach(layer, _glWidget->getMap()->getLayers()) {
        TrackLayerPtr trk = std::dynamic_pointer_cast<TrackLayer>(layer);
        if (trk)
            applyTrackStyleRule(rules, trk.get());
    }
    _glWidget->update();
}

bool
MainWindow::slotSaveMapFile()
{
    if (_fileIO->getFilename().isEmpty())
        return slotSaveMapFileAs();
    _fileIO->setViewArea(_glWidget->getViewArea());
    _fileIO->writeMapFile();
    return true;
}

bool
MainWindow::slotSaveMapFileAs()
{
    QString saveFileName = QFileDialog::getSaveFileName(this);
    if (saveFileName.isEmpty())
        return false;
    return saveMapFile(saveFileName);
}

bool
MainWindow::slotOpenMapFile()
{
    if (!confirmAbandonMap())
        return false;
    QString path = QFileDialog::getOpenFileName(this, "Select map file");
    /* pump the event loop once to let the dialog disappear */
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return slotOpenMapFile(path);
}

bool
MainWindow::slotOpenMapFile(const QString &path)
{
    if (path.isEmpty())
        return false;
    clearMap();
    _addPathToRecentMenu(_recentMapsMenu, path);
    return loadMapFile(path);
}

void
MainWindow::slotNewMap()
{
    if (!confirmAbandonMap())
        return;
    clearMap();
}

void
MainWindow::_loadBaseMap()
{
    OsmLayer *layer = new OsmLayer();
    layer->setVisible(_settings->getShowOpenStreetMap());
    _glWidget->getMap()->addLayer(layer);
}

void
MainWindow::_addPathToRecentMenu(QMenu *theMenu, const QString &path)
{
    QFileInfo thisFile(path);
    QList<QAction*> recents = theMenu->actions();
    QAction *action;
    QAction *recentSep = NULL;
    foreach(action, recents) {
        if (action->data().toString() == path) {
            return;
        } else if (action->isSeparator()) {
            recentSep = action;
        }
    }
    
    if (recents.length() > _numRecentFiles) {
        theMenu->removeAction(recents.first());
    }
    
    QAction *thisAction = new QAction(thisFile.fileName(), this);
    thisAction->setData(QVariant(path));
    if (recentSep) {
        theMenu->insertAction(recentSep, thisAction);
    } else {
        theMenu->addAction(thisAction);
        theMenu->addSeparator();
        if (theMenu == _recentMapsMenu)
            theMenu->addAction(_clearRecentMapsMenuAction);
        else if (theMenu == _recentLayersMenu)
            theMenu->addAction(_clearRecentLayersMenuAction);
    }
}

void
MainWindow::slotAddLayer()
{
    QStringList paths = QFileDialog::getOpenFileNames(this,
                                        "Select track files (.gpx, .tcx, .fit)",
                                        QString() /*dir*/,
                                        "Tracklogs (*.gpx *.tcx *.fit)");
    if (!paths.isEmpty()) {
        QString path;
        foreach(path, paths) {
            slotAddLayer(path);
            _addPathToRecentMenu(_recentLayersMenu, path);
        }
    }
}

void
MainWindow::slotAddLayer(const QString &path)
{
    loadTrackFile(path);
    _fileIO->addTrackFile(path);
    /* remember the last layer path explicitly added because we'll
     frame up all tracks in it when the map is ready */
    _lastLayerPathAdded = path;
}

void
MainWindow::slotPlaybackRateChanged(double newRate)
{
    _glWidget->setPlaybackRate(newRate);
}

void
MainWindow::slotTimeChanged(double mapSeconds)
{
    _playbackWidget->setTime(mapSeconds);
}

void
MainWindow::onTimeSliderDrag(int seconds)
{
    _glWidget->setMapSeconds((double)seconds);
}

void
MainWindow::slotLayerAdded(LayerId layerId)
{
    int dur = _glWidget->getMap()->getDuration();
    if (dur > 0)
        _playbackWidget->setTimeSliderMaximum(dur);
    Layer *layer = _glWidget->getMap()->getLayer(layerId);
    if (layer) {
        _layerListWidget->addLayer(layer);
        
        if (!_lastLayerPathAdded.isEmpty() &&
            layer->sourceFilePath() == _lastLayerPathAdded)
            _layersToFrame.append(layer->id());
    }
    
    /* If the user explicitly added a path and the map is done projecting,
       frame all the layers that came from that path.
     */
    if (_settings->getFrameLastAddedLayer() &&
        !_layersToFrame.empty() && _layersToFrame.length() == _numPendingLayers)
    {
        slotFrameLayers(_layersToFrame);
        _layerListWidget->slotSetSelectedLayers(_layersToFrame);
        _lastLayerPathAdded.clear();
        _layersToFrame.clear();
        _numPendingLayers = 0;
    }
    /* pump the event loop */
    qApp->processEvents();
}

void
MainWindow::slotFrameLayers(const QList<LayerId> layerIds)
{
    _glWidget->slotFrameLayers(layerIds);
}

void
MainWindow::slotLayerSelectionChanged(const QList<unsigned int> layerIds)
{
    _glWidget->slotSelectLayers(layerIds);
}

void
MainWindow::slotLayerVisibilityChanged(LayerId layerId, bool visible)
{
    Layer *layer = _glWidget->getMap()->getLayer(layerId);
    if (layer)
        layer->setVisible(visible);
    _glWidget->update();
}

void
MainWindow::slotShowAboutDialog()
{
    _aboutDialog->show();
}

void
MainWindow::slotShowPreferencesDialog()
{
    _preferencesDialog->loadSettings();
    if (_preferencesDialog->exec() == QDialog::Accepted) {
        _preferencesDialog->saveSettings();
        TrackStyleRules rules = _preferencesDialog->getTrackStyleRules();
        _settings->setTrackStyleRules(rules);
        applyTrackStyleRules(rules);
    }
}

void
MainWindow::slotShowPlaybackWidget()
{
    _showWidget(_playbackWidget);
}

void
MainWindow::slotShowLayerListWidget()
{
    _showWidget(_layerListWidget);
}

void
MainWindow::slotShowMapWindow()
{
    _showWidget(_glWidget);
}

void
MainWindow::slotTrackFileLoaded(const QString &path, QList<Track*> *tracks)
{
    _trackFiles.append(path);
    Track *thisTrack;
    QList<Layer*> trackLayers;
    TrackStyleRules rules = _settings->getTrackStyleRules();
    foreach(thisTrack, *tracks) {
        TrackLayer *thisLayer = new TrackLayer(thisTrack);
        // See if the layer came from an explicitly-added path
        applyTrackStyleRule(rules, thisLayer);
        if (!_lastLayerPathAdded.isEmpty() &&
            thisLayer->sourceFilePath() == _lastLayerPathAdded)
            _numPendingLayers++;
        trackLayers.append(thisLayer);
        qApp->processEvents();
    }
    _glWidget->getMap()->addLayers(trackLayers);
}

void
MainWindow::slotTrackFileLoadError(const QString &path, const QString &what)
{
    QString err = QString("Error loading file '%0': %1").arg(path).arg(what);
    QMessageBox::critical(_glWidget, "Could not load file", err);
}

void
MainWindow::slotLockViewToSelectedLayer()
{
    QList<LayerId> layers = _layerListWidget->selectedLayerIds();
    if (layers.empty())
        QMessageBox::critical(_glWidget, "No layers selected",
                              "No layers selected to follow");
    else
        _glWidget->slotLockViewToLayer(layers.last());
}

void
MainWindow::slotFrameSelectedLayers()
{
    QList<LayerId> layers = _layerListWidget->selectedLayerIds();
    if (layers.empty())
        QMessageBox::critical(_glWidget, "No layers selected",
                              "No layers selected to frame");
    else
        slotFrameLayers(layers);
}

void
MainWindow::slotOpenRecentMapFile(QAction *mapAction)
{
    QString path = mapAction->data().toString();
    if (path.isEmpty() || !confirmAbandonMap())
        return;
    slotOpenMapFile(path);
}

void
MainWindow::slotAddRecentLayer(QAction *layerAction)
{
    QString path = layerAction->data().toString();
    if (!path.isEmpty())
        slotAddLayer(path);
}

void
MainWindow::slotSetStartingViewArea()
{
    LonLatBox area = _glWidget->getViewArea();
    _settings->setStartingViewArea(area);
}

void
MainWindow::slotAboutToQuit()
{
    saveSettings();
}

void
MainWindow::_showWidget(QWidget *widget)
{
    widget->show();
    widget->raise();
    widget->setWindowState(Qt::WindowActive);
}
