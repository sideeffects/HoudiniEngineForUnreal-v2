import unreal

""" Example for getting the API instance and starting/creating the Houdini
Engine Session.

"""


def run():
    # Get the API singleton
    api = unreal.HoudiniPublicAPIBlueprintLib.get_api()
    # Check if there is an existing valid session
    if not api.is_session_valid():
        # Create a new session
        api.create_session()


if __name__ == '__main__':
    run()
