"""Run in Unreal Editor Python; preserves crowbar grip and elbow trajectory."""
import unreal, math, json, os
anim = unreal.load_asset('/Game/FirstPerson/Anims/Combat/AS_Crowbar_Attack')
ext = unreal.AnimPoseExtensions
options = unreal.AnimPoseEvaluationOptions()
count = anim.data_model_interface.get_number_of_frames() + 1
rate = anim.data_model_interface.get_frame_rate()
fps = rate.numerator / rate.denominator
poses = [ext.get_anim_pose_at_time(anim, i/fps, options) for i in range(count)]
updates = {}
def inverse(q):
    return unreal.Quat(-q.x, -q.y, -q.z, q.w)
for side in ['l', 'r']:
    previous_angle = None
    for pose in poses:
        hand = ext.get_bone_pose(pose, 'hand_'+side)
        forearm = ext.get_bone_pose(pose, 'lowerarm_'+side)
        q = hand.rotation
        theta = 2*math.atan2(q.x, q.w)
        if previous_angle is None:
            theta = (theta+math.pi)%(2*math.pi)-math.pi
        else:
            theta = previous_angle+(theta-previous_angle+math.pi)%(2*math.pi)-math.pi
        previous_angle = theta
        roll = unreal.Quat(math.sin(theta/2),0,0,math.cos(theta/2))
        rotations = {'lowerarm_'+side: forearm.rotation*roll, 'hand_'+side: inverse(roll)*q}
        # Helper bones are siblings at 1/3 and 2/3 along the forearm.
        for suffix, weight in [('02', 2/3), ('01', 1/3)]:
            name = 'lowerarm_twist_'+suffix+'_'+side
            ref = ext.get_ref_bone_pose(pose, name)
            a = -theta*weight/2
            rotations[name] = unreal.Quat(math.sin(a),0,0,math.cos(a))*ref.rotation
        for name, rotation in rotations.items():
            t = ext.get_bone_pose(pose, name)
            data = updates.setdefault(name, {'p': [], 'q': [], 's': []})
            if data['q']:
                old = data['q'][-1]
                if old.x*rotation.x+old.y*rotation.y+old.z*rotation.z+old.w*rotation.w < 0:
                    rotation = unreal.Quat(-rotation.x,-rotation.y,-rotation.z,-rotation.w)
            data['p'].append(t.translation)
            data['q'].append(rotation.normalized())
            data['s'].append(t.scale3d)
controller = anim.controller
controller.open_bracket('Distribute crowbar wrist roll along forearms')
try:
    for bone, data in updates.items():
        assert controller.set_bone_track_keys(bone, data['p'], data['q'], data['s'])
finally:
    controller.close_bracket()
max_position_error = max_rotation_error = max_wrist_roll = 0.
for i, before in enumerate(poses):
    after = ext.get_anim_pose_at_time(anim, i/fps, options)
    for side in ['l', 'r']:
        for name in ['hand_'+side, 'lowerarm_'+side]:
            a = ext.get_bone_pose(before, name, unreal.AnimPoseSpaces.WORLD)
            b = ext.get_bone_pose(after, name, unreal.AnimPoseSpaces.WORLD)
            max_position_error = max(max_position_error, (a.translation-b.translation).length())
            if name.startswith('hand'):
                delta = inverse(a.rotation)*b.rotation
                max_rotation_error = max(max_rotation_error, math.degrees(2*math.acos(min(1.,abs(delta.w)))))
        q = ext.get_bone_pose(after, 'hand_'+side).rotation
        max_wrist_roll = max(max_wrist_roll, abs(q.x))
assert max_position_error < .01, max_position_error
assert max_rotation_error < .1, max_rotation_error
assert max_wrist_roll < .001, max_wrist_roll
assert unreal.EditorAssetLibrary.save_loaded_asset(anim)
report = dict(frames=count, max_position_error_cm=max_position_error, max_grip_rotation_error_degrees=max_rotation_error, max_wrist_quaternion_x=max_wrist_roll)
with open(os.path.join(unreal.Paths.project_saved_dir(), 'Backups/CrowbarWristFix_20261120/validation.json'), 'w') as f:
    json.dump(report, f, indent=2)
print(report)
