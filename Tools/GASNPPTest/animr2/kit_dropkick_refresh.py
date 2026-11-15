import runpy
g = runpy.run_path('C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Tools/GAS/setup_overlord_dropkick_kit.py', run_name='dkkit')
print('refresh result', g['run'](refresh=True))
