""" An example script that spawns some actors and then uses the API to 
instantiate an HDA and set the actors as world inputs. The inputs are set 
during post instantiation (before the first cook). After the first cook and 
output creation (post processing) the input structure is fetched and logged.

"""

import unreal

_g_wrapper = None


def get_test_hda_path():
    return '/HoudiniEngine/Examples/hda/subnet_test_2_0.subnet_test_2_0'


def get_test_hda():
    return unreal.load_object(None, get_test_hda_path())


def get_geo_asset_path():
    return '/Engine/BasicShapes/Cube.Cube'


def get_geo_asset():
    return unreal.load_object(None, get_geo_asset_path())


def get_cylinder_asset_path():
    return '/Engine/BasicShapes/Cylinder.Cylinder'


def get_cylinder_asset():
    return unreal.load_object(None, get_cylinder_asset_path())


def configure_inputs(in_wrapper):
    print('configure_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_instantiation_delegate.remove_callable(configure_inputs)

    # Spawn some actors
    actors = spawn_actors()

    # Create a world input
    world_input = in_wrapper.create_empty_input(unreal.HoudiniPublicAPIWorldInput)
    # Set the input objects/assets for this input
    world_input.set_input_objects(actors)
    # copy the input data to the HDA as node input 0
    in_wrapper.set_input_at_index(0, world_input)
    # We can now discard the API input object
    world_input = None

    # Set the subnet_test HDA to output its first input
    in_wrapper.set_int_parameter_value('enable_geo', 1)


def print_api_input(in_input):
    print('\t\tInput type: {0}'.format(in_input.__class__))
    print('\t\tbKeepWorldTransform: {0}'.format(in_input.keep_world_transform))
    print('\t\tbImportAsReference: {0}'.format(in_input.import_as_reference))
    if isinstance(in_input, unreal.HoudiniPublicAPIWorldInput):
        print('\t\tbPackBeforeMerge: {0}'.format(in_input.pack_before_merge))
        print('\t\tbExportLODs: {0}'.format(in_input.export_lo_ds))
        print('\t\tbExportSockets: {0}'.format(in_input.export_sockets))
        print('\t\tbExportColliders: {0}'.format(in_input.export_colliders))
        print('\t\tbIsWorldInputBoundSelector: {0}'.format(in_input.is_world_input_bound_selector))
        print('\t\tbWorldInputBoundSelectorAutoUpdate: {0}'.format(in_input.world_input_bound_selector_auto_update))

    input_objects = in_input.get_input_objects()
    if not input_objects:
        print('\t\tEmpty input!')
    else:
        print('\t\tNumber of objects in input: {0}'.format(len(input_objects)))
        for idx, input_object in enumerate(input_objects):
            print('\t\t\tInput object #{0}: {1}'.format(idx, input_object))
            if hasattr(in_input, 'get_object_transform_offset'):
                print('\t\t\tObject Transform Offset: {0}'.format(in_input.get_object_transform_offset(input_object)))


def print_inputs(in_wrapper):
    print('print_inputs')

    # Unbind from the delegate
    in_wrapper.on_post_processing_delegate.remove_callable(print_inputs)

    # Fetch inputs, iterate over it and log
    node_inputs = in_wrapper.get_inputs_at_indices()
    parm_inputs = in_wrapper.get_input_parameters()

    if not node_inputs:
        print('No node inputs found!')
    else:
        print('Number of node inputs: {0}'.format(len(node_inputs)))
        for input_index, input_wrapper in node_inputs.items():
            print('\tInput index: {0}'.format(input_index))
            print_api_input(input_wrapper)

    if not parm_inputs:
        print('No parameter inputs found!')
    else:
        print('Number of parameter inputs: {0}'.format(len(parm_inputs)))
        for parm_name, input_wrapper in parm_inputs.items():
            print('\tInput parameter name: {0}'.format(parm_name))
            print_api_input(input_wrapper)


def spawn_actors():
    actors = []
    # Spawn a static mesh actor and assign a cylinder to its static mesh
    # component
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, location=(0, 0, 0))
    actor.static_mesh_component.set_static_mesh(get_cylinder_asset())
    actor.set_actor_label('Cylinder')
    actor.set_actor_transform(
        unreal.Transform(
            (-200, 0, 0),
            (0, 0, 0),
            (2, 2, 2),
        ),
        sweep=False,
        teleport=True
    )
    actors.append(actor)

    # Spawn a static mesh actor and assign a cube to its static mesh
    # component
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, location=(0, 0, 0))
    actor.static_mesh_component.set_static_mesh(get_geo_asset())
    actor.set_actor_label('Cube')
    actor.set_actor_transform(
        unreal.Transform(
            (200, 0, 100),
            (45, 0, 45),
            (2, 2, 2),
        ),
        sweep=False,
        teleport=True
    )
    actors.append(actor)

    return actors


def run():
    # get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()

    global _g_wrapper
    # instantiate an asset with auto-cook enabled
    _g_wrapper = api.instantiate_asset(get_test_hda(), unreal.Transform())

    # Configure inputs on_post_instantiation, after instantiation, but before first cook
    _g_wrapper.on_post_instantiation_delegate.add_callable(configure_inputs)
    # Bind on_post_processing, after cook + output creation
    _g_wrapper.on_post_processing_delegate.add_callable(print_inputs)


if __name__ == '__main__':
    run()
