""" Example script that instantiates an HDA using the API and then
setting some parameter values after instantiation but before the 
first cook.

"""

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/pig_head_subdivider_v01.pig_head_subdivider_v01'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def on_post_instantiation(in_wrapper):
    print('on_post_instantiation')
    # in_wrapper.on_post_instantiation_delegate.remove_callable(on_post_instantiation)

    # Set some parameters to create instances and enable a material
    in_wrapper.set_bool_parameter_value('add_instances', True)
    in_wrapper.set_int_parameter_value('num_instances', 8)
    in_wrapper.set_bool_parameter_value('addshader', True)


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Bind on_post_instantiation (before the first cook) callback to set parameters
    _g_wrapper.on_post_instantiation_delegate.add_callable(on_post_instantiation)


if __name__ == '__main__':
    run()
