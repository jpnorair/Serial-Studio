/*
 * Copyright (c) 2020-2021 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Generator.h"

#include <Logger.h>
#include <CSV/Player.h>
#include <IO/Manager.h>
#include <Misc/Utilities.h>
#include <ConsoleAppender.h>

#include <QFileInfo>
#include <QFileDialog>

using namespace JSON;

/*
 * Only instance of the class
 */
static Generator *INSTANCE = nullptr;

/*
 * Regular expresion used to check if there are still unmatched values
 * on the JSON map file.
 */
static const QRegExp UNMATCHED_VALUES_REGEX("(%\b([0-9]|[1-9][0-9])\b)");


// Prototypes for local functions
void modifyJsonValue(QJsonValue& destValue, const QString& path, const QJsonValue& newValue);
void modifyJsonValue(QJsonDocument* doc, const QString& path, const QJsonValue& newValue);
void processFrame(const QByteArray &data, const quint64 frame, const QDateTime &time, QJSEngine* engine);


/**
 * Initializes the JSON Parser class and connects appropiate SIGNALS/SLOTS
 * @todo implement destructor to delete m_template
 */
Generator::Generator()
    : m_frameCount(0)
    , m_jsonTemplateMutex(QMutex::NonRecursive)
    , m_opMode(kAutomatic)
{
    auto io = IO::Manager::getInstance();
    auto cp = CSV::Player::getInstance();
    connect(cp, SIGNAL(openChanged()), this, SLOT(reset()));
    connect(io, SIGNAL(deviceChanged()), this, SLOT(reset()));

    connect(io, SIGNAL(frameReceived(QByteArray)), this, SLOT(readData(QByteArray)));

    m_workerThread.start();

    LOG_TRACE() << "Class initialized";
}

/**
 * Returns the only instance of the class
 */
Generator *Generator::getInstance()
{
    if (!INSTANCE)
        INSTANCE = new Generator();

    return INSTANCE;
}

/**
 * Returns the JSON map data from the loaded file as a string
 */
QString Generator::jsonMapData() const
{
    return m_jsonMapData;
}

/**
 * Returns the file name (e.g. "JsonMap.json") of the loaded JSON map file
 */
QString Generator::jsonMapFilename() const
{
    if (m_jsonMap.isOpen())
    {
        auto fileInfo = QFileInfo(m_jsonMap.fileName());
        return fileInfo.fileName();
    }

    return "";
}

QJsonDocument* Generator::openJsonTemplate() {
    m_jsonTemplateMutex.lock();
    return &m_jsonTemplate;
}

void Generator::closeJsonTemplate() {
    m_jsonTemplateMutex.unlock();
}

void Generator::saveJsonTemplate(QJsonDocument &tmpl) {
    ///Do Mutex
    //m_jsonTemplate = tmpl;
}


/**
 * Returns the file path of the loaded JSON map file
 */
QString Generator::jsonMapFilepath() const
{
    if (m_jsonMap.isOpen())
    {
        auto fileInfo = QFileInfo(m_jsonMap.fileName());
        return fileInfo.filePath();
    }

    return "";
}

/**
 * Returns the operation mode
 */
Generator::OperationMode Generator::operationMode() const
{
    return m_opMode;
}

/**
 * Creates a file dialog & lets the user select the JSON file map
 */
void Generator::loadJsonMap()
{
    // clang-format off
//    auto file = QFileDialog::getOpenFileName(Q_NULLPTR,
//                                             tr("Select JSON map file"),
//                                             QDir::homePath(),
//                                             tr("JSON files") + " (*.json)");
    auto file = QFileDialog::getOpenFileName(Q_NULLPTR,
                                             tr("Select JSON map file or JS Script"),
                                             QDir::homePath(),
                                             tr("JSON or JS files") + " (*.json *.js)");


    if (!file.isEmpty()) {
        loadJsonMap(file);

        if (operationMode() == kScript) {
            // If the data template is empty, get the template by passing-in an empty string.
            // Get the data template by passing-in an empty string
            QJSEngine engine_tmpl;
            QJSValue result_tmpl;
            QJSValue js_tmpl = engine_tmpl.evaluate(Generator::getInstance()->jsonMapData().toUtf8());
            QJSValueList tmpl_args;
            tmpl_args << QString::fromUtf8("");
            result_tmpl = js_tmpl.call(tmpl_args);

            if ((result_tmpl.isError() == false) && (result_tmpl.isObject() == true)) {
                //m_jsonTemplate = QJsonObject(result_tmpl.toVariant().toJsonObject());
                m_jsonTemplate = QJsonDocument::fromVariant(result_tmpl.toVariant());
            }
        }
    }
}

/**
 * Opens, validates & loads into memory the JSON file in the given @a path.
 */
void Generator::loadJsonMap(const QString &path, const bool silent)
{
    // Log information
    LOG_TRACE() << "Loading JSON/JS file, silent flag set to" << silent;

    // Validate path
    if (path.isEmpty())
        return;

    // Close previous file (if open)
    if (m_jsonMap.isOpen())
    {
        m_jsonMap.close();
        emit jsonFileMapChanged();
    }

    // Try to open the file (read only mode)
    m_jsonMap.setFileName(path);
    if (m_jsonMap.open(QFile::ReadOnly))
    {
        // Read data & validate JSON from file
        ///@todo handle JS document
        QJsonParseError error;
        auto data = m_jsonMap.readAll();

        // Validate the JSON/JS
        ///@todo there is no JS validation here, yet
        if (Generator::getInstance()->operationMode() == Generator::kManual) {
            auto document = QJsonDocument::fromJson(data, &error);
            // Get rid of warnings
            Q_UNUSED(document);
        }
        else {
            error.error = QJsonParseError::NoError;
        }

        // Put up an alert box if there are errors in the JSON/JS
        // If no errors, push the data forward
        if (error.error != QJsonParseError::NoError) {
            LOG_TRACE() << "JSON parse error" << error.errorString();

            m_jsonMap.close();
            writeSettings("");
            Misc::Utilities::showMessageBox(tr("JSON parse error"), error.errorString());
        }
        else
        {
            LOG_TRACE() << "JSON map loaded successfully";

            writeSettings(path);
            m_jsonMapData = QString::fromUtf8(data);
            if (!silent)
                Misc::Utilities::showMessageBox(
                    tr("JSON map file loaded successfully!"),
                    tr("File \"%1\" loaded into memory").arg(jsonMapFilename()));
        }
    }

    // Open error
    else
    {
        LOG_TRACE() << "JSON file error" << m_jsonMap.errorString();

        writeSettings("");
        Misc::Utilities::showMessageBox(tr("Cannot read JSON file"),
                                        tr("Please check file permissions & location"));
        m_jsonMap.close();
    }

    // Update UI
    emit jsonFileMapChanged();
}

/**
 * Changes the operation mode of the JSON parser. There are two possible op.
 * modes:
 *
 * @c kManual serial data only contains the comma-separated values, and we need
 *            to use a JSON map file (given by the user) to know what each value
 *            means. This method is recommended when we need to transfer &
 *            display a large amount of information from the microcontroller
 *            unit to the computer.
 *
 * @c kAutomatic serial data contains the JSON data frame, good for simple
 *               applications or for prototyping.
 *
 * @c kScript Serial data is arbitrary apart from being new-line terminated.
 *            A custom script (JS) will convert each line into JSON.
 */
void Generator::setOperationMode(const OperationMode mode)
{
    m_opMode = mode;
    emit operationModeChanged();

    LOG_TRACE() << "Operation mode set to" << mode;
}

/**
 * Loads the last saved JSON map file (if any)
 */
void Generator::readSettings()
{
    auto path = m_settings.value("json_map_location", "").toString();
    if (!path.isEmpty())
        loadJsonMap(path, true);
}

/**
 * Saves the location of the last valid JSON map file that was opened (if any)
 */
void Generator::writeSettings(const QString &path)
{
    m_settings.setValue("json_map_location", path);
}

/**
 * Notifies the rest of the application that a new JSON frame has been received. The JFI
 * also contains RX date/time and frame number.
 *
 * Read the "FrameInfo.h" file for more information.
 */
void Generator::loadJFI(const JFI_Object &info)
{
    bool csvOpen = CSV::Player::getInstance()->isOpen();
    bool devOpen = IO::Manager::getInstance()->connected();

    if (csvOpen || devOpen)
    {
        if (JFI_Valid(info))
            emit jsonChanged(info);
    }

    else
        reset();
}

/**
 * Create a new JFI event with the given @a JSON document and increment the frame count
 * --> This is used only by the replay feature
 */
void Generator::loadJSON(const QJsonDocument &json)
{
    auto jfi = JFI_CreateNew(m_frameCount, QDateTime::currentDateTime(), json);
    m_frameCount++;
    loadJFI(jfi);
}

/**
 * Resets all the statistics related to the current device and the JSON map file
 * If there's a JSON template, then we reload it.
 */
void Generator::reset()
{
    m_frameCount = 0;

    emit jsonChanged(JFI_Empty());
}

/**
 * Tries to parse the given data as a JSON document according to the selected
 * operation mode.
 *
 * Possible operation modes:
 * - Auto:   serial data contains the JSON data frame
 * - Manual: serial data only contains the comma-separated values, and we need
 *           to use a JSON map file (given by the user) to know what each value
 *           means
 *
 * If JSON parsing is successfull, then the class shall notify the rest of the
 * application in order to process packet data.
 */
void Generator::readData(const QByteArray &data)
{
    // CSV-replay active, abort
    if (CSV::Player::getInstance()->isOpen())
        return;

    // Data empty, abort
    if (data.isEmpty()) {
        return;
    }

    // Increment received frames
    m_frameCount++;
    //LOG_INFO() << "Frame Count:" << m_frameCount;

    QJSEngine* engine = nullptr;
    processFrame(data, m_frameCount, QDateTime::currentDateTime(), engine);

    // This uses a separate thread to process the input.
    // Doing so can create out-of-order problems.
    /*
    // Create new worker thread to read JSON data
    QThread *thread = new QThread;
    JSONWorker *worker = new JSONWorker(data, m_frameCount, QDateTime::currentDateTime());
    worker->moveToThread(thread);

    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, &JSONWorker::jsonReady, this, &Generator::loadJFI);
    thread->start();
    */
}

//----------------------------------------------------------------------------------------
// JSON worker object (executed for each frame on a new thread)
//----------------------------------------------------------------------------------------

/**
 * Constructor function, stores received frame data & the date/time that the frame data
 * was received.
 */
JSONWorker::JSONWorker(const QByteArray &data, const quint64 frame, const QDateTime &time)
    : m_time(time)
    , m_data(data)
    , m_frame(frame)
    , m_engine(nullptr)
{
}

/**
 * Reads the frame & inserts its values on the JSON map, and/or extracts the JSON frame
 * directly from the serial data.
 */
void JSONWorker::process()
{
    // Init variables
    QJsonParseError error;
    QJsonDocument document;

    // Serial device sends JSON (auto mode)
    if (Generator::getInstance()->operationMode() == Generator::kAutomatic)
        document = QJsonDocument::fromJson(m_data, &error);

    // We need to use a map file, check if its loaded & replace values into map
    else if (Generator::getInstance()->operationMode() == Generator::kManual)
    {
        // Initialize javscript engine
        m_engine = new QJSEngine(this);

        // Empty JSON map data
        if (Generator::getInstance()->jsonMapData().isEmpty())
            return;

        // Init conversion status boolean
        bool ok = true;

        // Separate incoming data & add it to the JSON map
        auto json = Generator::getInstance()->jsonMapData();
        auto list = QString::fromUtf8(m_data).split(',');
        for (int i = 0; i < list.count(); ++i)
        {
            // Get value at i & insert it into json
            auto str = list.at(i);
            auto mod = json.arg(str);

            // If JSON after insertion is different we're good to go
            if (json != mod)
                json = mod;

            // JSON is the same after insertion -> format error
            else
            {
                ok = false;
                break;
            }
        }

        // Test that JSON does not contain unmatched values
        if (ok)
            ok = !(json.contains(UNMATCHED_VALUES_REGEX));

        // There was an error & the JSON map is incomplete (or misses received
        // info from the microcontroller).
        if (!ok)
            return;

        // Create json document
        auto jsonDocument = QJsonDocument::fromJson(json.toUtf8(), &error);

        // Calculate dynamically generated values
        auto root = jsonDocument.object();
        auto groups = root.value("g").toArray();
        for (int i = 0; i < groups.count(); ++i)
        {
            // Get group
            auto group = groups.at(i).toObject();

            // Evaluate each dataset of the current group
            auto datasets = group.value("d").toArray();
            for (int j = 0; j < datasets.count(); ++j)
            {
                // Get dataset object & value
                auto dataset = datasets.at(j).toObject();
                auto value = dataset.value("v").toString();

                // Evaluate code in dataset value (if any)
                auto jsValue = m_engine->evaluate(value);

                // Code execution correct, replace value in JSON
                if (!jsValue.isError())
                {
                    dataset.remove("v");
                    dataset.insert("v", jsValue.toString());
                    datasets.replace(j, dataset);
                }
            }

            // Replace group datasets
            group.remove("d");
            group.insert("d", datasets);
            groups.replace(i, group);
        }

        // Replace root document group objects
        root.remove("g");
        root.insert("g", groups);

        // Create JSON document
        document = QJsonDocument(root);

        // Delete javacript engine
        m_engine->deleteLater();
    }

    // We need to use a custom script to parse the input
    // Generator::getInstance()->operationMode() == Generator::kScript
    ///@todo It would be nice to only uptake and compile the script once,
    ///      then reuse the jsfn object on new input data.
    else {
        //LOG_INFO() << "Data ingested:" << m_data;
        // Exit on Empty JS script
        if (Generator::getInstance()->jsonMapData().isEmpty())
            return;

        // Call the script again on real data
        m_engine = new QJSEngine(this);

        // "jsfn" is a function created when the QJSEngine compiles the script text.
        QJSValue jsfn = m_engine->evaluate(Generator::getInstance()->jsonMapData().toUtf8());
        QJSValue result;

        QJSValueList args;
        args << QString::fromUtf8(m_data);
        result = jsfn.call(args);

        if (result.isError()) {
            //LOG_INFO() << "Script failed";
            error.error = QJsonParseError::MissingObject;
        }
        else {
            // Default error if no matching data: missing object
            error.error = QJsonParseError::MissingObject;

            // There are two options
            // 1. use the template, which accepts input as partial dataset
            // 2. don't use a template, thus input must contain the entire dataset
            //LOG_INFO() << document.toJson().simplified();

            // Overlay new values into the template.
            // This is a deep loop that matches the input data hierachy against the template data hierarchy.
            // Where there is overlay, the data values are written to the template.
            // If there is any success, the updated template is converted to output document.
            QJsonDocument* tmpl = Generator::getInstance()->openJsonTemplate();
            //LOG_INFO() << "tmpl" << tmpl;

            if (!tmpl->isEmpty()) {
                bool change_made = false;
                QJsonObject data = QJsonObject(result.toVariant().toJsonObject());

                // Make sure top-level data formats are matching
                if (QString::compare(data["t"].toString(), (*tmpl)["t"].toString()) == 0) {
                    //LOG_INFO() << "data[t]" << data["t"].toString();

                    if (data["g"].isArray() && (*tmpl)["g"].isArray()) {
                        QJsonArray t_groups = (*tmpl)["g"].toArray();
                        QJsonArray d_groups = data["g"].toArray();
                        int tg, dg;
                        for (tg = 0; tg < t_groups.count(); ++tg) {
                            if (!t_groups[tg].isObject()) continue;
                            if (!t_groups[tg].toObject().contains("d")) continue;
                            if (!t_groups[tg].toObject()["d"].isArray()) continue;

                            for (dg = 0; dg < d_groups.count(); ++dg) {
                                if (!d_groups[dg].isObject()) continue;
                                if (!d_groups[dg].toObject().contains("d")) continue;
                                if (!d_groups[dg].toObject()["d"].isArray()) continue;

                                // Make sure group types are matching
                                if (QString::compare(d_groups[dg].toObject()["t"].toString(), t_groups[tg].toObject()["t"].toString()) != 0)
                                    continue;

                                QJsonArray tg_data = t_groups[tg].toObject()["d"].toArray();
                                QJsonArray dg_data = d_groups[dg].toObject()["d"].toArray();
                                int tgd, dgd;
                                for (tgd = 0; tgd < tg_data.count(); ++tgd ) {
                                    if (!tg_data[tgd].isObject()) continue;
                                    if (!(tg_data[tgd].toObject().contains("t") && tg_data[tgd].toObject().contains("v"))) continue;

                                    for (dgd = 0; dgd != dg_data.count(); ++dgd ) {
                                        if (!dg_data[dgd].isObject()) continue;
                                        if (!(dg_data[dgd].toObject().contains("t") && dg_data[dgd].toObject().contains("v"))) continue;

                                        // Make sure data item types are matching
                                        if (QString::compare(dg_data[dgd].toObject()["t"].toString(), tg_data[tgd].toObject()["t"].toString()) != 0)
                                            continue;

                                        // Finally! Copy data from input to template
                                        modifyJsonValue(tmpl, QString("g[%1].d[%2].v").arg(tg).arg(tgd), dg_data[dgd].toObject()["v"]);

                                        // If data contains the optional x (time) parameter, copy that too
                                        if (tg_data[tgd].toObject().contains("x") && dg_data[dgd].toObject().contains("x")) {
                                            if (dg_data[dgd].toObject()["x"].isDouble()) {
                                                modifyJsonValue(tmpl, QString("g[%1].d[%2].x").arg(tg).arg(tgd), dg_data[dgd].toObject()["x"]);
                                            }
                                        }

                                        ///@todo need to copy this back into tmpl itself

                                        // Need to flag that a change was made in order for document to be outputted
                                        change_made = true;
                                    }
                                }
                            }
                        }
                    }
                    if (change_made) {
                        document = *tmpl;
                    }
                }
                // Give Mutex
            }

            // Do not use a template, just package the object generated by the script
            else {
                document = QJsonDocument::fromVariant(result.toVariant());
            }

            if (!document.isEmpty()) {
                //LOG_INFO() << document.toJson().simplified();
                error.error = QJsonParseError::NoError;
            }

            Generator::getInstance()->closeJsonTemplate();
        }

        m_engine->deleteLater();
    }

    // No parse error, update UI & reset error counter
    if (error.error == QJsonParseError::NoError) {
        emit jsonReady(JFI_CreateNew(m_frame, m_time, document));
    }

    // Delete object in 500 ms
    QTimer::singleShot(500, this, SIGNAL(finished()));
}



void processFrame(const QByteArray &data, const quint64 frame, const QDateTime &time, QJSEngine *engine) {
    // Init variables
    QJsonParseError error;
    QJsonDocument document;

    // Serial device sends JSON (auto mode)
    if (Generator::getInstance()->operationMode() == Generator::kAutomatic)
        document = QJsonDocument::fromJson(data, &error);

    // We need to use a map file, check if its loaded & replace values into map
    else if (Generator::getInstance()->operationMode() == Generator::kManual)
    {
        // Empty JSON map data
        if (Generator::getInstance()->jsonMapData().isEmpty())
            return;

        // Initialize javscript engine
        engine = new QJSEngine();

        // Init conversion status boolean
        bool ok = true;

        // Separate incoming data & add it to the JSON map
        auto json = Generator::getInstance()->jsonMapData();
        auto list = QString::fromUtf8(data).split(',');
        for (int i = 0; i < list.count(); ++i)
        {
            // Get value at i & insert it into json
            auto str = list.at(i);
            auto mod = json.arg(str);

            // If JSON after insertion is different we're good to go
            if (json != mod)
                json = mod;

            // JSON is the same after insertion -> format error
            else
            {
                ok = false;
                break;
            }
        }

        // Test that JSON does not contain unmatched values
        if (ok)
            ok = !(json.contains(UNMATCHED_VALUES_REGEX));

        // There was an error & the JSON map is incomplete (or misses received
        // info from the microcontroller).
        if (!ok)
            return;

        // Create json document
        auto jsonDocument = QJsonDocument::fromJson(json.toUtf8(), &error);

        // Calculate dynamically generated values
        auto root = jsonDocument.object();
        auto groups = root.value("g").toArray();
        for (int i = 0; i < groups.count(); ++i)
        {
            // Get group
            auto group = groups.at(i).toObject();

            // Evaluate each dataset of the current group
            auto datasets = group.value("d").toArray();
            for (int j = 0; j < datasets.count(); ++j)
            {
                // Get dataset object & value
                auto dataset = datasets.at(j).toObject();
                auto value = dataset.value("v").toString();

                // Evaluate code in dataset value (if any)
                auto jsValue = engine->evaluate(value);

                // Code execution correct, replace value in JSON
                if (!jsValue.isError())
                {
                    dataset.remove("v");
                    dataset.insert("v", jsValue.toString());
                    datasets.replace(j, dataset);
                }
            }

            // Replace group datasets
            group.remove("d");
            group.insert("d", datasets);
            groups.replace(i, group);
        }

        // Replace root document group objects
        root.remove("g");
        root.insert("g", groups);

        // Create JSON document
        document = QJsonDocument(root);

        // Delete javacript engine
        engine->deleteLater();
    }

    // We need to use a custom script to parse the input
    // Generator::getInstance()->operationMode() == Generator::kScript
    ///@todo It would be nice to only uptake and compile the script once,
    ///      then reuse the jsfn object on new input data.
    else {
        //LOG_INFO() << "Data ingested:" << m_data;
        // Exit on Empty JS script
        if (Generator::getInstance()->jsonMapData().isEmpty())
            return;

        // Call the script again on real data
        engine = new QJSEngine();

        // "jsfn" is a function created when the QJSEngine compiles the script text.
        QJSValue jsfn = engine->evaluate(Generator::getInstance()->jsonMapData().toUtf8());
        QJSValue result;

        QJSValueList args;
        args << QString::fromUtf8(data);
        result = jsfn.call(args);

        if (result.isError()) {
            //LOG_INFO() << "Script failed";
            error.error = QJsonParseError::MissingObject;
        }
        else {
            // Default error if no matching data: missing object
            error.error = QJsonParseError::MissingObject;

            // There are two options
            // 1. use the template, which accepts input as partial dataset
            // 2. don't use a template, thus input must contain the entire dataset
            //LOG_INFO() << document.toJson().simplified();

            // Overlay new values into the template.
            // This is a deep loop that matches the input data hierachy against the template data hierarchy.
            // Where there is overlay, the data values are written to the template.
            // If there is any success, the updated template is converted to output document.
            QJsonDocument* tmpl = Generator::getInstance()->openJsonTemplate();
            //LOG_INFO() << "tmpl" << tmpl;

            if (!tmpl->isEmpty()) {
                bool change_made = false;
                QJsonObject data = QJsonObject(result.toVariant().toJsonObject());

                // Make sure top-level data formats are matching
                if (QString::compare(data["t"].toString(), (*tmpl)["t"].toString()) == 0) {
                    //LOG_INFO() << "data[t]" << data["t"].toString();

                    if (data["g"].isArray() && (*tmpl)["g"].isArray()) {
                        QJsonArray t_groups = (*tmpl)["g"].toArray();
                        QJsonArray d_groups = data["g"].toArray();
                        int tg, dg;
                        for (tg = 0; tg < t_groups.count(); ++tg) {
                            if (!t_groups[tg].isObject()) continue;
                            if (!t_groups[tg].toObject().contains("d")) continue;
                            if (!t_groups[tg].toObject()["d"].isArray()) continue;

                            for (dg = 0; dg < d_groups.count(); ++dg) {
                                if (!d_groups[dg].isObject()) continue;
                                if (!d_groups[dg].toObject().contains("d")) continue;
                                if (!d_groups[dg].toObject()["d"].isArray()) continue;

                                // Make sure group types are matching
                                if (QString::compare(d_groups[dg].toObject()["t"].toString(), t_groups[tg].toObject()["t"].toString()) != 0)
                                    continue;

                                QJsonArray tg_data = t_groups[tg].toObject()["d"].toArray();
                                QJsonArray dg_data = d_groups[dg].toObject()["d"].toArray();
                                int tgd, dgd;
                                for (tgd = 0; tgd < tg_data.count(); ++tgd ) {
                                    if (!tg_data[tgd].isObject()) continue;
                                    if (!(tg_data[tgd].toObject().contains("t") && tg_data[tgd].toObject().contains("v"))) continue;

                                    for (dgd = 0; dgd != dg_data.count(); ++dgd ) {
                                        if (!dg_data[dgd].isObject()) continue;
                                        if (!(dg_data[dgd].toObject().contains("t") && dg_data[dgd].toObject().contains("v"))) continue;

                                        // Make sure data item types are matching
                                        if (QString::compare(dg_data[dgd].toObject()["t"].toString(), tg_data[tgd].toObject()["t"].toString()) != 0)
                                            continue;

                                        // Finally! Copy data from input to template
                                        modifyJsonValue(tmpl, QString("g[%1].d[%2].v").arg(tg).arg(tgd), dg_data[dgd].toObject()["v"]);

                                        // If data contains the optional x (time) parameter, copy that too
                                        if (tg_data[tgd].toObject().contains("x") && dg_data[dgd].toObject().contains("x")) {
                                            if (dg_data[dgd].toObject()["x"].isDouble()) {
                                                modifyJsonValue(tmpl, QString("g[%1].d[%2].x").arg(tg).arg(tgd), dg_data[dgd].toObject()["x"]);
                                            }
                                        }

                                        ///@todo need to copy this back into tmpl itself

                                        // Need to flag that a change was made in order for document to be outputted
                                        change_made = true;
                                    }
                                }
                            }
                        }
                    }
                    if (change_made) {
                        document = *tmpl;
                    }
                }
                // Give Mutex
            }

            // Do not use a template, just package the object generated by the script
            else {
                document = QJsonDocument::fromVariant(result.toVariant());
            }

            if (!document.isEmpty()) {
                //LOG_INFO() << document.toJson().simplified();
                error.error = QJsonParseError::NoError;
            }

            Generator::getInstance()->closeJsonTemplate();
        }

        engine->deleteLater();
    }

    // No parse error, update UI & reset error counter
    if (error.error == QJsonParseError::NoError) {
        Generator::getInstance()->loadJFI(JFI_CreateNew(frame, time, document));
    }
}



// Local functions outside a class

void modifyJsonValue(QJsonValue& destValue, const QString& path, const QJsonValue& newValue)
{
    const int indexOfDot = path.indexOf('.');
    const QString dotPropertyName = path.left(indexOfDot);
    const QString dotSubPath = indexOfDot > 0 ? path.mid(indexOfDot + 1) : QString();

    const int indexOfSquareBracketOpen = path.indexOf('[');
    const int indexOfSquareBracketClose = path.indexOf(']');

    const int arrayIndex = path.mid(indexOfSquareBracketOpen + 1, indexOfSquareBracketClose - indexOfSquareBracketOpen - 1).toInt();

    const QString squareBracketPropertyName = path.left(indexOfSquareBracketOpen);
    const QString squareBracketSubPath = indexOfSquareBracketClose > 0 ? (path.mid(indexOfSquareBracketClose + 1)[0] == '.' ? path.mid(indexOfSquareBracketClose + 2) : path.mid(indexOfSquareBracketClose + 1)) : QString();

    // determine what is first in path. dot or bracket
    bool useDot = true;
    if (indexOfDot >= 0) // there is a dot in path
    {
        if (indexOfSquareBracketOpen >= 0) // there is squarebracket in path
        {
            if (indexOfDot > indexOfSquareBracketOpen)
                useDot = false;
            else
                useDot = true;
        }
        else
            useDot = true;
    }
    else
    {
        if (indexOfSquareBracketOpen >= 0)
            useDot = false;
        else
            useDot = true; // acutally, id doesn't matter, both dot and square bracket don't exist
    }

    QString usedPropertyName = useDot ? dotPropertyName : squareBracketPropertyName;
    QString usedSubPath = useDot ? dotSubPath : squareBracketSubPath;

    QJsonValue subValue;
    if (destValue.isArray())
        subValue = destValue.toArray()[usedPropertyName.toInt()];
    else if (destValue.isObject())
        subValue = destValue.toObject()[usedPropertyName];
    else
        qDebug() << "oh, what should i do now with the following value?! " << destValue;

    if(usedSubPath.isEmpty())
    {
        subValue = newValue;
    }
    else
    {
        if (subValue.isArray())
        {
            QJsonArray arr = subValue.toArray();
            QJsonValue arrEntry = arr[arrayIndex];
            modifyJsonValue(arrEntry,usedSubPath,newValue);
            arr[arrayIndex] = arrEntry;
            subValue = arr;
        }
        else if (subValue.isObject())
            modifyJsonValue(subValue,usedSubPath,newValue);
        else
            subValue = newValue;
    }

    if (destValue.isArray())
    {
        QJsonArray arr = destValue.toArray();
        if (subValue.isNull())
            arr.removeAt(arrayIndex);
        else
            arr[arrayIndex] = subValue;
        destValue = arr;
    }
    else if (destValue.isObject())
    {
        QJsonObject obj = destValue.toObject();
        if (subValue.isNull())
            obj.remove(usedPropertyName);
        else
            obj[usedPropertyName] = subValue;
        destValue = obj;
    }
    else
        destValue = newValue;
}

void modifyJsonValue(QJsonDocument* doc, const QString& path, const QJsonValue& newValue)
{
    QJsonValue val;
    if (doc->isArray())
        val = doc->array();
    else
        val = doc->object();

    modifyJsonValue(val,path,newValue);

    if (val.isArray())
        *doc = QJsonDocument(val.toArray());
    else
        *doc = QJsonDocument(val.toObject());
}
