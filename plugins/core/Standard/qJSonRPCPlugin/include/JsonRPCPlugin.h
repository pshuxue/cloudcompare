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

#pragma once

#include "ccStdPluginInterface.h"
#include "jsonrpcserver.h"

//! Json RPC Plugin
class JsonRPCPlugin : public QObject, public ccStdPluginInterface
{
	Q_OBJECT
	Q_INTERFACES( ccPluginInterface ccStdPluginInterface )

	Q_PLUGIN_METADATA( IID "cccorp.cloudcompare.plugin.JsonRPC" FILE "../info.json" )

public:
	explicit JsonRPCPlugin( QObject *parent = nullptr );
	~JsonRPCPlugin() override = default;

	// Inherited from ccStdPluginInterface
	QList<QAction *> getActions() override;
public slots:
    void triggered(bool checked);
	/// slot for execution
    JsonRPCResult execute(QString method, QMap<QString, QVariant> params);

private:
	//! Default action
    QAction* m_action{nullptr};
    const QString version{"1.2"};

    QString recursiveName(ccHObject *);
    ccHObject *createParent(QString path);
protected:
	/// server that emit the execute signals
    JsonRPCServer rpc_server;
};
