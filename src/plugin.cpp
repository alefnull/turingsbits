#include "plugin.hpp"

Plugin *pluginInstance;

void init(Plugin *p)
{
	pluginInstance = p;
	p->addModel(modelNala);
	p->addModel(modelAbra);
	p->addModel(modelCadabra);
}
