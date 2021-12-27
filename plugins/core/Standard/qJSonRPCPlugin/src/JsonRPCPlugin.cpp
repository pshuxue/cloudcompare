//##########################################################################
//#                                                                        #
//#                CLOUDCOMPARE PLUGIN: JsonRPCPlugin                      #
//#                                                                        #
//#  This program is free software; you can redistribute it and/or modify  #
//#  it under the terms of the GNU General Public License as published by  #
//#  the Free Software Foundation; version 2 of the License.               #
//#                                                                        #
//#  This program is distributed in the hope that it will be useful,       #
//#  but WITHOUT ANY WARRANTY; without even the implied warranty of        #
//#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
//#  GNU General Public License for more details.                          #
//#                                                                        #
//#                             COPYRIGHT: theAdib, 2020                   #
//#                                                                        #
//##########################################################################

//Qt
#include <QtGui>

//local
#include "JsonRPCPlugin.h"

//qCC_db
#include <ccGenericPointCloud.h>
//qCC_gl
#include <ccGLWindow.h>
//qCC_io
#include <FileIOFilter.h>
//CC plugins
#include <ccMainAppInterface.h>

JsonRPCPlugin::JsonRPCPlugin( QObject *parent )
	: QObject( parent )
	, ccStdPluginInterface( ":/CC/plugin/JsonRPCPlugin/info.json" )
{
    qDebug() << "JsonRPCPlugin::JsonRPCPlugin";

    connect(&rpc_server, &JsonRPCServer::execute, this, &JsonRPCPlugin::execute);
}

QList<QAction *> JsonRPCPlugin::getActions()
{
    qDebug() << "JsonRPCPlugin::getActions";

	// default action (if it has not been already created, this is the moment to do it)
	if ( !m_action )
	{
		m_action = new QAction( getName(), this );
		m_action->setToolTip( getDescription() );
        m_action->setIcon( getIcon() );
        m_action->setCheckable(true);
        m_action->setChecked(false);
		m_action->setEnabled(true);

		// Connect appropriate signal
        connect( m_action, &QAction::triggered, this, &JsonRPCPlugin::triggered);
	}

    return { m_action };
}

void JsonRPCPlugin::triggered(bool checked)
{
    qDebug() << "JsonRPCPlugin::triggered " << checked;

    if (checked)
	{
        rpc_server.listen(6001);
    }
	else
	{
        rpc_server.close();
    }
}

JsonRPCResult JsonRPCPlugin::execute(QString method, QMap<QString, QVariant> params)
{
    qDebug() << method << params;
    if (m_app == nullptr)
	{
        return JsonRPCResult();
    }

    JsonRPCResult result;
    bool need_redraw = false;
    if (method == "open")
	{
        QString filename = params["filename"].toString();
        // code copied from MainWindow::addToDB()
        //to use the same 'global shift' for multiple files
		CCVector3d loadCoordinatesShift(0, 0, 0);
        bool loadCoordinatesTransEnabled = false;

        FileIOFilter::LoadParameters parameters;
        {
            parameters.alwaysDisplayLoadDialog = true;
            parameters.shiftHandlingMode = ccGlobalShiftManager::DIALOG_IF_NECESSARY;
            parameters.coordinatesShift = &loadCoordinatesShift;
            parameters.coordinatesShiftEnabled = &loadCoordinatesTransEnabled;
            parameters.parentWidget = m_app->getActiveGLWindow()->asWidget();
        }

        if(params.contains("silent") && (params["silent"].toBool() == true))
		{
            parameters.alwaysDisplayLoadDialog = false;
        }
        CC_FILE_ERROR res = CC_FERR_NO_ERROR;
        ccHObject* newGroup = FileIOFilter::LoadFromFile(filename, parameters, res, params["filter"].toString());

        if (newGroup)
		{
            //disable the normals on all loaded clouds!
            ccHObject::Container clouds;
            newGroup->filterChildren(clouds, true, CC_TYPES::POINT_CLOUD);
            for (ccHObject* cloud : clouds)
			{
                if (cloud) 
				{
                    static_cast<ccGenericPointCloud*>(cloud)->showNormals(false);
                }
            }

            // apply matrix if possible
            QList<QVariant> transformation = params["transformation"].toList();
            if (transformation.size() == 4*4)
			{
                std::vector<double> values(4*4);
                bool success;
				for (unsigned i = 0; i < 4 * 4; ++i)
				{
                    double d = transformation[i].toDouble(&success);
                    qDebug() << transformation[i].toString() << transformation[i].toDouble();
                    if (!success)
					{
						break;
                    }
					values[((i % 4) * 4) + (i / 4)] = d;
                }
                if (success)
				{
                    ccGLMatrix mat = ccGLMatrix(values.data());
                    qDebug() << "apply matrix: " << mat.toString();
                    newGroup->setGLTransformation(mat);
                }
            }

            // the object name in CC treeview
            QString objname = params["name"].toString().trimmed();
            if(!objname.isEmpty())
            {
                newGroup->setName(objname);
            }


            QString parentname = params["parent"].toString().trimmed();
            if(!parentname.isEmpty())
            {
                auto parent = createParent(parentname);
                if(parent)
                {
                    parent->addChild(newGroup);
                    // in addChild the display assignment to child is only to the first child, not recursivly
                    const auto display = m_app->getActiveGLWindow();
                    if(display) {
                        newGroup->setDisplay_recursive(display);
                    }
                }
            }

            m_app->addToDB(newGroup);

            // after adding Zoom complete Scene into current display
            auto display = m_app->getActiveGLWindow();
            if(display) {
                display->zoomGlobal();
            }

            need_redraw = true;
            result = JsonRPCResult::success(0);
        }
		else
		{
            result = JsonRPCResult::error(1, "cancelled by user");
        }
    }
    else if (method == "clear")
    {

        // remove everything below root
        auto root = m_app->dbRootObject();
        ccHObject* child;
        while ((child = root->getChild(0)) != nullptr)
        {
            m_app->removeFromDB(child, true);
        }
        need_redraw = true;
        result = JsonRPCResult::success(0);
    }
    else if (method == "version")
    {
        // return version number of plugin
        result = JsonRPCResult::success(version);
    }

	// redraw
    if (need_redraw)
	{
        ccGLWindow* win = m_app->getActiveGLWindow();
        if (win)
            win->redraw();
    }

    return result;
}

// concatenate the names recursively
QString JsonRPCPlugin::recursiveName(ccHObject *obj)
{
    QString name;

    while(obj) {
        name.prepend(obj->getName()+"/");
        obj = obj->getParent();
    }

    return name;
}


// select objects along the path and create if not existing
ccHObject *JsonRPCPlugin::createParent(QString path)
{
    QStringList tokens = path.split("/");
    qDebug() << "createParent() " << path << tokens;
    auto parent = m_app->dbRootObject();
    while(!tokens.isEmpty() && parent)
    {
        if(!tokens.first().trimmed().isEmpty())
        {
            ccHObject *next{nullptr};
            ccHObject::Container filteredChildren;
            parent->filterChildren(filteredChildren, false, CC_TYPES::HIERARCHY_OBJECT, true);
            for(const auto &child: filteredChildren)
            {
                qDebug() << "iterate name:" << child->getName() << ", type:" << child->getClassID() << " recursive: " << recursiveName(child);

                if(child->getName().compare(tokens.first().trimmed(), Qt::CaseInsensitive) == 0) {
                    // name matches
                    next = child;
                    break;
                }
            }
            if(!next)
            {
                // not found; create one
                next = new ccHObject(tokens.first().trimmed());
                if(!next) {
                    return parent;
                }
                parent->addChild(next);
                m_app->addToDB(next);
            }
            parent = next;
        }
        tokens.removeFirst();
    }
    return parent;
}
