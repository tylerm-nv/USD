#!/pxrpythonsubst
#
# Copyright 2018 Pixar
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
#    names, trademarks, service marks, or product names of the Licensor
#    and its affiliates, except as required to comply with Section 4(c) of
#    the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.

import os
os.environ['PXR_USDMDL_PLUGIN_SEARCH_PATHS'] = os.path.join(os.getcwd(), 'mdl')

from pxr import Tf, Ndr, Sdr
import unittest

class TestDiscovery(unittest.TestCase):
    def test_NodeDiscovery(self):
        """
        Test MDL node discovery.
        """
        registry = Sdr.Registry()

        # Check node identifiers.
        names = sorted(
            filter(lambda x: x.startswith('::nvidia::'),registry.GetNodeIdentifiers()))

        mdlIdentifiers = \
            ['::nvidia::core_definitions::add_cutout', '::nvidia::core_definitions::add_displacement', '::nvidia::core_definitions::add_emission', '::nvidia::core_definitions::add_globalbump', '::nvidia::core_definitions::add_simple_sticker', '::nvidia::core_definitions::add_thermal_emission', '::nvidia::core_definitions::apply_clearcoat', '::nvidia::core_definitions::apply_colorfalloff', '::nvidia::core_definitions::apply_colorfalloff_v2', '::nvidia::core_definitions::apply_dustcover', '::nvidia::core_definitions::apply_metalcoat', '::nvidia::core_definitions::apply_metallicflakes', '::nvidia::core_definitions::apply_thinfilm', '::nvidia::core_definitions::blend', '::nvidia::core_definitions::blend_colors(color,color,::base::color_layer_mode,float)', '::nvidia::core_definitions::checker_bump_texture(float,float,bool,float3,float3,float3,int)', '::nvidia::core_definitions::checker_texture(color,color,float3,float3,bool,float,float3,int)', '::nvidia::core_definitions::diffuse', '::nvidia::core_definitions::file_bump_texture(texture_2d,::base::mono_mode,float2,float2,float,bool,float,int)', '::nvidia::core_definitions::file_texture(texture_2d,::base::mono_mode,float,float,float2,float2,float,bool,int,bool)', '::nvidia::core_definitions::flake_paint', '::nvidia::core_definitions::flex_material', '::nvidia::core_definitions::flow_noise_bump_texture(float,float3,int,bool,int,bool,float,float,float,float,float,float3,float3)', '::nvidia::core_definitions::flow_noise_texture(color,color,bool,int,int,bool,float,float,float,float,float,float3,float3,float3)', '::nvidia::core_definitions::int(::nvidia::core_definitions::cell_base)', '::nvidia::core_definitions::int(::nvidia::core_definitions::cell_type)', '::nvidia::core_definitions::int(::nvidia::core_definitions::emission_type)', '::nvidia::core_definitions::int(::nvidia::core_definitions::material_type)', '::nvidia::core_definitions::light_ies', '::nvidia::core_definitions::light_omni', '::nvidia::core_definitions::light_spot', '::nvidia::core_definitions::metal', '::nvidia::core_definitions::normalmap_texture(texture_2d,float2,float2,float,bool,float,int)', '::nvidia::core_definitions::perlin_noise_bump_texture(float,float3,int,bool,bool,float,float,float3,float3,int)', '::nvidia::core_definitions::perlin_noise_texture(color,color,bool,int,bool,float,float,float3,float3,float3,int)', '::nvidia::core_definitions::plastic', '::nvidia::core_definitions::retroreflective', '::nvidia::core_definitions::rotation_translation_scale(float3,float3,float3)', '::nvidia::core_definitions::scratched_metal', '::nvidia::core_definitions::scratched_plastic', '::nvidia::core_definitions::texture_return.mono(::base::texture_return)', '::nvidia::core_definitions::texture_return.tint(::base::texture_return)', '::nvidia::core_definitions::thick_glass', '::nvidia::core_definitions::thick_translucent', '::nvidia::core_definitions::thin_glass', '::nvidia::core_definitions::thin_translucent', '::nvidia::core_definitions::worley_noise_bump_texture(float,::nvidia::core_definitions::cell_base,bool,int,float,float,float3,float3,float3)', '::nvidia::core_definitions::worley_noise_texture(color,color,bool,int,::nvidia::core_definitions::cell_type,::nvidia::core_definitions::cell_base,float,float,float3,float3,float3)']
        self.assertEqual(names, mdlIdentifiers)

if __name__ == '__main__':
    unittest.main()
