#!/usr/bin/env python3
"""
PA3 Evaluator - Global Routing Evaluator and Visualizer
Usage: python pa3_evaluator.py <cap_file> <net_file> <route_file>
Usage with plotting: python pa3_evaluator.py -plot <cap_file> <net_file> <route_file>
"""

import sys
import os


# ============================================================================
# PARSER FUNCTIONS
# ============================================================================

def parse_cap_file(filepath):
    with open(filepath, 'r') as f:
        lines = [line.strip() for line in f.readlines() if line.strip()]
    
    idx = 0
    # line 1: nLayers xSize ySize
    nLayers, xSize, ySize = map(int, lines[idx].split())
    idx += 1
    
    # line 2: unit_via_cost
    unit_via_cost = int(lines[idx])
    idx += 1

    # line 3: horizontal_edge_lengths
    horizontal_edge_lengths = list(map(int, lines[idx].split()))
    idx += 1
    
    # line 4: vertical_edge_lengths  
    vertical_edge_lengths = list(map(int, lines[idx].split()))
    idx += 1
    
    # Read layer data
    layers = []
    for _ in range(nLayers):
        # Layer header: name direction
        header = lines[idx].split()
        layer_name = header[0]
        layer_direction = header[1]
        idx += 1
        
        # Capacity grid
        capacities = []
        for row in range(ySize):
            row_data = list(map(int, lines[idx].split()))
            capacities.append(row_data)
            idx += 1
        
        layers.append({
            'name': layer_name,
            'direction': layer_direction,
            'capacities': capacities
        })
    
    return {
        'nLayers': nLayers,
        'xSize': xSize,
        'ySize': ySize,
        'unit_length_wire_cost': 1,
        'unit_via_cost': unit_via_cost,
        'horizontal_edge_lengths': horizontal_edge_lengths,
        'vertical_edge_lengths': vertical_edge_lengths,
        'layers': layers
    }


def parse_net_file(filepath):
    with open(filepath, 'r') as f:
        lines = [line.strip() for line in f.readlines()]
    
    nets = []
    i = 0
    
    while i < len(lines):
        line = lines[i]
        
        # skip empty lines
        if not line:
            i += 1
            continue
        
        # Net name
        if not line.startswith('(') and line != ')':
            net_name = line
            pins = []
            i += 1
            
            # skip the starting '('
            if i < len(lines) and lines[i] == '(':
                i += 1
                
                # read each pin's coordinates
                while i < len(lines) and lines[i] != ')':
                    line = lines[i]
                    if line.startswith('(') and line.endswith(')'):
                        # parse (layer, x, y)
                        coords = line[1:-1].split(',')
                        layer, x, y = int(coords[0]), int(coords[1]), int(coords[2])
                        pins.append((layer, x, y))
                    i += 1
                
                # skip the ending ')'
                if i < len(lines) and lines[i] == ')':
                    i += 1
            
            nets.append({
                'name': net_name,
                'pins': pins
            })
        else:
            i += 1
    
    return nets


def parse_route_file(filepath):
    """
    Parse .route file
    Format: 
        net_name
        (
        z1 x1 y1 z2 x2 y2
        z1 x1 y1 z2 x2 y2
        ...
        )
    
    Returns:
        list of dicts with keys:
            - name: net name
            - segments: list of segments, each segment is (x1, y1, z1, x2, y2, z2)
    """
    with open(filepath, 'r') as f:
        lines = [line.strip() for line in f.readlines()]
    
    nets = []
    i = 0
    
    while i < len(lines):
        line = lines[i]
        
        # skip empty lines
        if not line:
            i += 1
            continue
        
        # Net name
        if not line.startswith('(') and line != ')':
            net_name = line
            segments = []
            i += 1
            
            # skip the starting '('
            if i < len(lines) and lines[i] == '(':
                i += 1
                
                # read each segment
                while i < len(lines) and lines[i] != ')':
                    line = lines[i]
                    if line:
                        # parse z1 x1 y1 z2 x2 y2
                        coords = list(map(int, line.split()))
                        if len(coords) == 6:
                            z1, x1, y1, z2, x2, y2 = coords
                            segments.append((x1, y1, z1, x2, y2, z2))
                    i += 1
                
                # skip the ending ')'
                if i < len(lines) and lines[i] == ')':
                    i += 1
            
            nets.append({
                'name': net_name,
                'segments': segments
            })
        else:
            i += 1
    
    return nets


# ============================================================================
# EVALUATOR FUNCTIONS
# ============================================================================

def evaluate_route(cap_data, route_data):
    """
    Evaluate routing result
    
    Args:
        cap_data: capacity data from parse_cap_file
        route_data: route data from parse_route_file
    
    Returns:
        dict with keys:
            - overflow: total overflow (sum of max(demand - capacity, 0) for all edges)
            - total_cost: total wire cost (wirelength cost + via cost)
            - wirelength_cost: cost from wire segments using GCellEdgeLengths
            - via_cost: cost from vias
            - num_vias: number of vias
            - wirelength: total physical wirelength
    """
    xSize = cap_data['xSize']
    ySize = cap_data['ySize']
    nLayers = cap_data['nLayers']
    unit_via_cost = cap_data['unit_via_cost']
    horizontal_edge_lengths = cap_data['horizontal_edge_lengths']
    vertical_edge_lengths = cap_data['vertical_edge_lengths']
    
    # Initialize demand arrays for each layer
    demand = []
    for layer_idx in range(nLayers):
        layer = cap_data['layers'][layer_idx]
        direction = layer['direction']
        
        if direction == 'H':
            layer_demand = [[0 for _ in range(xSize)] for _ in range(ySize)]
        else:  # 'V'
            layer_demand = [[0 for _ in range(xSize)] for _ in range(ySize)]
        
        demand.append(layer_demand)
    
    # Count wire segments and vias
    num_vias = 0
    total_wirelength = 0
    
    # Process each segment
    for net in route_data:
        # Use a set to track which GCells this net has already used
        # to avoid counting overlapping segments in the same net twice
        # Format: (layer, row, col) for both wires and vias
        used_gcells = set()
        
        for x1, y1, z1, x2, y2, z2 in net['segments']:
            if z1 != z2:
                # This is a via - add demand to both layers at this position
                num_vias += 1
                for layer_idx in [z1, z2]:
                    gcell = (layer_idx, y1, x1)
                    if gcell not in used_gcells:
                        demand[layer_idx][y1][x1] += 1
                        used_gcells.add(gcell)
            else:
                # This is a wire segment
                layer_idx = z1
                layer = cap_data['layers'][layer_idx]
                direction = layer['direction']
                
                if direction == 'H':
                    # Horizontal wire: should be y1 == y2
                    if y1 == y2:
                        # Increment demand for this horizontal edge
                        y = y1
                        x_min = min(x1, x2)
                        x_max = max(x1, x2)
                        for x in range(x_min, x_max):
                            if 0 <= y < ySize and 0 <= x < xSize:
                                gcell = (layer_idx, y, x)
                                if gcell not in used_gcells:
                                    demand[layer_idx][y][x] += 1
                                    used_gcells.add(gcell)
                                # Add wirelength using horizontal edge length (always count)
                                total_wirelength += horizontal_edge_lengths[x]
                else:  # 'V'
                    # Vertical wire: should be x1 == x2
                    if x1 == x2:
                        # Increment demand for this vertical edge
                        x = x1
                        y_min = min(y1, y2)
                        y_max = max(y1, y2)
                        for y in range(y_min, y_max):
                            if 0 <= y < ySize and 0 <= x < xSize:
                                gcell = (layer_idx, y, x)
                                if gcell not in used_gcells:
                                    demand[layer_idx][y][x] += 1
                                    used_gcells.add(gcell)
                                # Add wirelength using vertical edge length (always count)
                                total_wirelength += vertical_edge_lengths[y]
    
    # Calculate overflow
    total_overflow = 0
    for layer_idx in range(nLayers):
        layer = cap_data['layers'][layer_idx]
        capacities = layer['capacities']
        
        for y in range(len(demand[layer_idx])):
            for x in range(len(demand[layer_idx][y])):
                cap = capacities[y][x]
                dem = demand[layer_idx][y][x]
                overflow = max(dem - cap, 0)
                total_overflow += overflow
    
    # Calculate costs
    wirelength_cost = total_wirelength
    via_cost_total = num_vias * unit_via_cost
    total_cost = wirelength_cost + via_cost_total
    
    return {
        'overflow': total_overflow,
        'total_cost': total_cost,
        'wirelength_cost': wirelength_cost,
        'via_cost': via_cost_total,
        'num_vias': num_vias,
        'wirelength': total_wirelength
    }


def print_evaluation(result):
    """Pretty print evaluation result"""
    print("=== Routing Evaluation ===")
    print(f"Total Overflow: {result['overflow']}")
    print(f"Total Cost: {result['total_cost']}")
    print(f"  - Wirelength Cost: {result['wirelength_cost']}")
    print(f"  - Via Cost: {result['via_cost']} ({result['num_vias']} vias)")


def check_route_validity(cap_data, route_data):
    """Check if route data is valid"""
    xSize = cap_data['xSize']
    ySize = cap_data['ySize']
    nLayers = cap_data['nLayers']
    
    invalid_nets = []
    details = {}
    
    for net in route_data:
        net_name = net['name']
        segments = net['segments']
        errors = []
        
        for seg_idx, segment in enumerate(segments):
            # Check format: should have 6 values
            if len(segment) != 6:
                errors.append(f"Segment {seg_idx}: invalid format (expected 6 values, got {len(segment)})")
                continue
            
            x1, y1, z1, x2, y2, z2 = segment
            
            # Check if all values are integers
            try:
                x1, y1, z1, x2, y2, z2 = int(x1), int(y1), int(z1), int(x2), int(y2), int(z2)
            except (ValueError, TypeError):
                errors.append(f"Segment {seg_idx}: non-integer values")
                continue
            
            # Check boundaries
            if not (0 <= x1 < xSize and 0 <= x2 < xSize):
                errors.append(f"Segment {seg_idx}: x out of bounds (x1={x1}, x2={x2}, xSize={xSize})")
            
            if not (0 <= y1 < ySize and 0 <= y2 < ySize):
                errors.append(f"Segment {seg_idx}: y out of bounds (y1={y1}, y2={y2}, ySize={ySize})")
            
            if not (0 <= z1 < nLayers and 0 <= z2 < nLayers):
                errors.append(f"Segment {seg_idx}: layer out of bounds (z1={z1}, z2={z2}, nLayers={nLayers})")
            
            # Check segment direction
            if z1 == z2:
                # Same layer - should be either horizontal or vertical
                layer = cap_data['layers'][z1]
                direction = layer['direction']
                
                if direction == 'H':
                    # Horizontal layer: should have y1 == y2, x1 != x2
                    if y1 != y2:
                        errors.append(f"Segment {seg_idx}: on H layer {z1} but y1={y1} != y2={y2}")
                    if x1 == x2:
                        errors.append(f"Segment {seg_idx}: on H layer {z1} but x1={x1} == x2={x2} (zero-length segment)")
                elif direction == 'V':
                    # Vertical layer: should have x1 == x2, y1 != y2
                    if x1 != x2:
                        errors.append(f"Segment {seg_idx}: on V layer {z1} but x1={x1} != x2={x2}")
                    if y1 == y2:
                        errors.append(f"Segment {seg_idx}: on V layer {z1} but y1={y1} == y2={y2} (zero-length segment)")
            else:
                # Via - should have same (x, y) coordinates
                if x1 != x2 or y1 != y2:
                    errors.append(f"Segment {seg_idx}: via but coordinates differ (({x1},{y1},{z1}) -> ({x2},{y2},{z2}))")
        
        if errors:
            details[net_name] = {'valid': False, 'errors': errors}
            invalid_nets.append(net_name)
        else:
            details[net_name] = {'valid': True, 'num_segments': len(segments)}
    
    return {
        'all_valid': len(invalid_nets) == 0,
        'invalid_nets': invalid_nets,
        'details': details
    }


def print_route_validity(result):
    """Pretty print route validity check result"""
    print("=== Route Validity Check ===")
    if result['all_valid']:
        print("✓ All routes are valid!")
    else:
        print(f"✗ {len(result['invalid_nets'])} net(s) have invalid routes:")
        for net_name in result['invalid_nets']:
            detail = result['details'][net_name]
            print(f"\n  Net: {net_name}")
            for error in detail['errors']:
                print(f"    - {error}")


def check_connectivity(net_data, route_data):
    """Check if all nets are properly connected"""
    disconnected_nets = []
    details = {}
    
    # Create a mapping from net name to pins
    net_pins = {net['name']: net['pins'] for net in net_data}
    
    # Create a mapping from net name to segments
    net_segments = {}
    for net in route_data:
        net_segments[net['name']] = net['segments']
    
    # Check each net
    for net_name, pins in net_pins.items():
        if len(pins) < 2:
            # Single pin net, consider it connected
            details[net_name] = {'connected': True, 'reason': 'single pin'}
            continue
        
        if len(pins) != 2:
            # More than 2 pins (shouldn't happen in 2-pin nets)
            details[net_name] = {'connected': False, 'reason': f'expected 2 pins, got {len(pins)}'}
            disconnected_nets.append(net_name)
            continue
        
        segments = net_segments.get(net_name, [])
        
        if len(segments) == 0:
            # No routing segments
            details[net_name] = {'connected': False, 'reason': 'no segments'}
            disconnected_nets.append(net_name)
            continue
        
        # Get the two pins (layer, x, y)
        pin1 = pins[0]
        pin2 = pins[1]
        
        # Check 1: First segment should have one end at pin1 or pin2
        first_seg = segments[0]
        x1, y1, z1, x2, y2, z2 = first_seg
        first_start = (z1, x1, y1)
        first_end = (z2, x2, y2)
        
        if first_start not in [pin1, pin2] and first_end not in [pin1, pin2]:
            details[net_name] = {'connected': False, 'reason': 'first segment not connected to any pin'}
            disconnected_nets.append(net_name)
            continue
        
        # Check 2: Each consecutive segment should share at least one point
        is_connected = True
        for i in range(len(segments) - 1):
            seg_curr = segments[i]
            seg_next = segments[i + 1]
            
            x1, y1, z1, x2, y2, z2 = seg_curr
            x3, y3, z3, x4, y4, z4 = seg_next
            
            curr_start = (z1, x1, y1)
            curr_end = (z2, x2, y2)
            next_start = (z3, x3, y3)
            next_end = (z4, x4, y4)
            
            # Check if they share at least one point
            if not (curr_start == next_start or curr_start == next_end or 
                    curr_end == next_start or curr_end == next_end):
                details[net_name] = {'connected': False, 'reason': f'segments {i} and {i+1} not connected'}
                disconnected_nets.append(net_name)
                is_connected = False
                break
        
        if not is_connected:
            continue
        
        # Check 3: Last segment should have one end at pin1 or pin2
        last_seg = segments[-1]
        x1, y1, z1, x2, y2, z2 = last_seg
        last_start = (z1, x1, y1)
        last_end = (z2, x2, y2)
        
        if last_start not in [pin1, pin2] and last_end not in [pin1, pin2]:
            details[net_name] = {'connected': False, 'reason': 'last segment not connected to any pin'}
            disconnected_nets.append(net_name)
            continue
        
        # All checks passed
        details[net_name] = {'connected': True, 'num_pins': 2, 'num_segments': len(segments)}
    
    return {
        'all_connected': len(disconnected_nets) == 0,
        'disconnected_nets': disconnected_nets,
        'details': details
    }


def print_connectivity(result):
    """Pretty print connectivity check result"""
    print("=== Connectivity Check ===")
    if result['all_connected']:
        print("✓ All nets are properly connected!")
    else:
        print(f"✗ {len(result['disconnected_nets'])} net(s) are NOT properly connected:")
        for net_name in result['disconnected_nets']:
            detail = result['details'][net_name]
            print(f"  - {net_name}: {detail.get('reason', 'disconnected')}")


# ============================================================================
# PLOTTING FUNCTIONS - 2D
# ============================================================================

def draw_input(cap_data, net_data, randomize_position=False):
    """Draw grid and all nets' pins"""
    import matplotlib.pyplot as plt
    from matplotlib.colors import hsv_to_rgb
    import numpy as np
    
    xSize = cap_data['xSize']
    ySize = cap_data['ySize']
    nLayers = cap_data['nLayers']
    
    # set color for each net
    num_nets = len(net_data)
    predefined_colors = ['#bf504e', '#507fbd', '#f69546', '#7e48a2', '#11af57']
    colors = []
    for i in range(num_nets):
        if i < len(predefined_colors):
            colors.append(predefined_colors[i])
        else:
            hue = (i - len(predefined_colors)) / max(1, num_nets - len(predefined_colors))
            rgb = hsv_to_rgb([hue, 0.8, 0.9])
            colors.append(rgb)
    
    # create subplot for each layer
    fig, axes = plt.subplots(1, nLayers, figsize=(8 * nLayers, 6))
    if nLayers == 1:
        axes = [axes]
    
    for layer_idx in range(nLayers):
        ax = axes[layer_idx]
        layer = cap_data['layers'][layer_idx]
        
        # set grid range
        ax.set_xlim(0, xSize)
        ax.set_ylim(0, ySize)
        ax.set_aspect('equal')
        ax.invert_yaxis()
        
        # draw grid lines
        ax.set_xticks(np.arange(0, xSize + 1, 1))
        ax.set_yticks(np.arange(0, ySize + 1, 1))
        ax.grid(which='both', linewidth=2, color='gray')
        
        ax.set_xlabel('X')
        ax.set_ylabel('Y', rotation=0)
        ax.set_title(f"Layer {layer_idx}: {layer['name']} ({layer['direction']})")
        
        # draw net's pins
        for net_idx, net in enumerate(net_data):
            color = colors[net_idx]
            net_pins_on_layer = [(x, y) for (l, x, y) in net['pins'] if l == layer_idx]
            
            if net_pins_on_layer:
                for x, y in net_pins_on_layer:
                    if randomize_position:
                        px = x + np.random.uniform(0.2, 0.8)
                        py = y + np.random.uniform(0.2, 0.8)
                    else:
                        px = x + 0.5
                        py = y + 0.5
                    
                    ax.plot(px, py, 's', color=color, markersize=12, 
                           markeredgecolor='black', markeredgewidth=1.5,
                           label=net['name'] if (x, y) == net_pins_on_layer[0] else '')
        
        # Give labels
        handles, labels = ax.get_legend_handles_labels()
        by_label = dict(zip(labels, handles))
        if by_label and layer_idx == 0:
            ax.legend(by_label.values(), by_label.keys(), 
                     loc='upper left', bbox_to_anchor=(1.05, 1))
    
    return fig, axes


def draw_route(fig, axes, cap_data, net_data, route_data, randomize_position=False):
    """Draw routing with pins and segments"""
    from matplotlib.colors import hsv_to_rgb
    from matplotlib.lines import Line2D
    
    xSize = cap_data['xSize']
    ySize = cap_data['ySize']
    nLayers = cap_data['nLayers']
    
    # Get colors for each net
    num_nets = len(net_data)
    predefined_colors = ['#bf504e', '#507fbd', '#f69546', '#7e48a2', '#11af57']
    net_colors = {}
    for net_idx, net in enumerate(net_data):
        if net_idx < len(predefined_colors):
            net_colors[net['name']] = predefined_colors[net_idx]
        else:
            hue = (net_idx - len(predefined_colors)) / max(1, num_nets - len(predefined_colors))
            rgb = hsv_to_rgb([hue, 0.8, 0.9])
            net_colors[net['name']] = rgb
    
    # Draw routing segments
    for net in route_data:
        net_name = net['name']
        color = net_colors.get(net_name, 'black')
        
        for x1, y1, z1, x2, y2, z2 in net['segments']:
            if nLayers > 1:
                ax = axes[z1]
            else:
                ax = axes
            
            px1 = x1 + 0.5
            py1 = y1 + 0.5
            px2 = x2 + 0.5
            py2 = y2 + 0.5
            
            if z1 != z2:
                # Draw via
                for z in [z1, z2]:
                    if nLayers > 1:
                        ax_via = axes[z]
                    else:
                        ax_via = axes
                    ax_via.plot(px1, py1, 'o', color=color, markersize=10, 
                               markeredgecolor='black', markeredgewidth=0.5, alpha=0.6, zorder=5)
            else:
                # Regular wire segment
                ax.plot([px1, px2], [py1, py2], '-', 
                       color=color, linewidth=3, alpha=0.6, zorder=4)
    
    # Add legend for symbols
    if nLayers > 1:
        first_ax = axes[0]
    else:
        first_ax = axes[0] if isinstance(axes, list) else axes
    
    symbol_legend = [
        Line2D([0], [0], marker='s', color='w', label='Pin',
               markerfacecolor='gray', markeredgecolor='black', 
               markeredgewidth=1.5, markersize=10),
        Line2D([0], [0], marker='o', color='w', label='Via',
               markerfacecolor='gray', markeredgecolor='black', 
               markeredgewidth=0.5, markersize=10),
        Line2D([0], [0], color='gray', linewidth=3, label='Wire')
    ]
    
    leg1 = first_ax.get_legend()
    leg2 = first_ax.legend(handles=symbol_legend, loc='upper left', 
                          bbox_to_anchor=(1.05, 0.5), frameon=True, title='Symbol')
    
    if leg1 is not None:
        first_ax.add_artist(leg1)
    
    return fig, axes


# ============================================================================
# PLOTTING FUNCTIONS - 3D
# ============================================================================

def draw_input_3d(cap_data, net_data, randomize_position=False):
    """Draw 3D visualization with pins"""
    import matplotlib.pyplot as plt
    from matplotlib.colors import hsv_to_rgb
    import numpy as np
    
    xSize = cap_data['xSize']
    ySize = cap_data['ySize']
    nLayers = cap_data['nLayers']
    
    # Set color for each net
    num_nets = len(net_data)
    predefined_colors = ['#bf504e', '#507fbd', '#f69546', '#7e48a2', '#11af57']
    colors = []
    for i in range(num_nets):
        if i < len(predefined_colors):
            colors.append(predefined_colors[i])
        else:
            hue = (i - len(predefined_colors)) / max(1, num_nets - len(predefined_colors))
            rgb = hsv_to_rgb([hue, 0.8, 0.9])
            colors.append(rgb)
    
    # Create 3D plot
    fig = plt.figure(figsize=(12, 10))
    ax = fig.add_subplot(111, projection='3d')
    
    # Turn off automatic grid
    ax.grid(False)
    
    # Draw grid on each layer
    for z in range(nLayers):
        for y in range(ySize + 1):
            ax.plot([0, xSize], [y, y], [z, z], 'gray', linewidth=0.5, alpha=0.3)
        for x in range(xSize + 1):
            ax.plot([x, x], [0, ySize], [z, z], 'gray', linewidth=0.5, alpha=0.3)
    
    # Draw vertical lines connecting layers
    for x in range(xSize + 1):
        for y in range(ySize + 1):
            ax.plot([x, x], [y, y], [0, nLayers - 1], 'gray', linewidth=0.3, alpha=0.2)
    
    # Draw pins
    for net_idx, net in enumerate(net_data):
        color = colors[net_idx]
        
        for layer, x, y in net['pins']:
            if randomize_position:
                px = x + np.random.uniform(0.2, 0.8)
                py = y + np.random.uniform(0.2, 0.8)
            else:
                px = x + 0.5
                py = y + 0.5
            pz = layer
            
            ax.scatter([px], [py], [pz], c=[color], s=200, marker='s',
                      edgecolors='black', linewidths=1.5, alpha=0.9,
                      label=net['name'] if (layer, x, y) == net['pins'][0] else '')
    
    # Set labels and limits
    ax.set_xlabel('X')
    ax.set_ylabel('Y')
    ax.set_zlabel('Layer')
    ax.set_xlim(0, xSize)
    ax.set_ylim(0, ySize)
    ax.set_zlim(-0.5, nLayers - 0.5)
    
    ax.set_box_aspect([xSize, ySize, nLayers])
    
    # Set pane colors
    ax.xaxis.pane.fill = True
    ax.yaxis.pane.fill = True
    ax.zaxis.pane.fill = True
    ax.xaxis.pane.set_facecolor('#f0f0f0')
    ax.yaxis.pane.set_facecolor('#f0f0f0')
    ax.zaxis.pane.set_facecolor('#f0f0f0')
    ax.xaxis.pane.set_alpha(0.3)
    ax.yaxis.pane.set_alpha(0.3)
    ax.zaxis.pane.set_alpha(0.3)
    
    ax.invert_yaxis()
    
    ax.set_xticks(range(0, xSize + 1))
    ax.set_yticks(range(0, ySize + 1))
    ax.set_zticks(range(0, nLayers))
    
    ax.set_title('3D Layout View')
    
    # Add legend
    handles, labels = ax.get_legend_handles_labels()
    by_label = dict(zip(labels, handles))
    if by_label:
        ax.legend(by_label.values(), by_label.keys(), loc='upper left')
    
    return fig, ax


def draw_route_3d(fig, ax, cap_data, net_data, route_data, randomize_position=False):
    """Draw routing segments on existing 3D plot"""
    from matplotlib.colors import hsv_to_rgb
    
    # Get colors for each net
    num_nets = len(net_data)
    predefined_colors = ['#bf504e', '#507fbd', '#f69546', '#7e48a2', '#11af57']
    net_colors = {}
    for net_idx, net in enumerate(net_data):
        if net_idx < len(predefined_colors):
            net_colors[net['name']] = predefined_colors[net_idx]
        else:
            hue = (net_idx - len(predefined_colors)) / max(1, num_nets - len(predefined_colors))
            rgb = hsv_to_rgb([hue, 0.8, 0.9])
            net_colors[net['name']] = rgb
    
    # Draw routing segments
    for net in route_data:
        net_name = net['name']
        color = net_colors.get(net_name, 'black')
        
        for x1, y1, z1, x2, y2, z2 in net['segments']:
            px1 = x1 + 0.5
            py1 = y1 + 0.5
            pz1 = z1
            px2 = x2 + 0.5
            py2 = y2 + 0.5
            pz2 = z2
            
            ax.plot([px1, px2], [py1, py2], [pz1, pz2], 
                   color=color, linewidth=3, alpha=0.6)
            
            if z1 != z2:
                ax.scatter([px1], [py1], [pz1], c=[color], s=100, marker='o',
                          edgecolors='black', linewidths=0.5, alpha=0.6)
                ax.scatter([px2], [py2], [pz2], c=[color], s=100, marker='o',
                          edgecolors='black', linewidths=0.5, alpha=0.6)
    
    return fig, ax


# ============================================================================
# MAIN FUNCTION
# ============================================================================

def main():
    # Separate -plot flag from file arguments
    args = sys.argv[1:]
    plot_flag = '-plot' in args
    
    # Remove -plot from arguments if present
    file_args = [arg for arg in args if arg != '-plot']
    
    if len(file_args) != 3:
        print("Usage: python pa3_evaluator.py <cap_file> <net_file> <route_file> [-plot]")
        print("  -plot can be placed at any position")
        sys.exit(1)
    
    cap_file = file_args[0]
    net_file = file_args[1]
    route_file = file_args[2]
    
    # Check if files exist
    for filepath in [cap_file, net_file, route_file]:
        if not os.path.exists(filepath):
            print(f"Error: File not found: {filepath}")
            sys.exit(1)
    
    print("="*60)
    print("PA3 Global Routing Evaluator")
    print("="*60)
    
    # Parse files
    print("\n[1/5] Parsing input files...")
    cap_data = parse_cap_file(cap_file)
    net_data = parse_net_file(net_file)
    route_data = parse_route_file(route_file)
    print(f"  Grid: {cap_data['xSize']} x {cap_data['ySize']}, Layers: {cap_data['nLayers']}")
    print(f"  Nets: {len(net_data)}")
    print(f"  Total segments: {sum(len(net['segments']) for net in route_data)}")
    
    # Check route validity
    print("\n[2/5] Checking route validity...")
    validity_result = check_route_validity(cap_data, route_data)
    print_route_validity(validity_result)
    
    # Check connectivity
    print("\n[3/5] Checking connectivity...")
    conn_result = check_connectivity(net_data, route_data)
    print_connectivity(conn_result)
    
    # Evaluate routing
    print("\n[4/5] Evaluating routing quality...")
    eval_result = evaluate_route(cap_data, route_data)
    print_evaluation(eval_result)
    
    # Generate plots (only if -plot flag is provided)
    if plot_flag:
        print("\n[5/5] Generating visualizations...")
        import matplotlib.pyplot as plt
        
        # Create plot directory if it doesn't exist
        plot_dir = "plot"
        os.makedirs(plot_dir, exist_ok=True)
        
        # Get base filename from route_file
        base_name = os.path.basename(route_file).replace('.route', '')
        
        # 2D plots
        print("  - Generating 2D plots...")
        fig, axes = draw_input(cap_data, net_data)
        fig, axes = draw_route(fig, axes, cap_data, net_data, route_data)
        plt.tight_layout()
        output_2d = os.path.join(plot_dir, f"{base_name}_2d.png")
        plt.savefig(output_2d, dpi=150, bbox_inches='tight')
        print(f"    Saved: {output_2d}")
        plt.close()
        
        # 3D plot
        print("  - Generating 3D plot...")
        fig, ax = draw_input_3d(cap_data, net_data)
        fig, ax = draw_route_3d(fig, ax, cap_data, net_data, route_data)
        plt.tight_layout()
        output_3d = os.path.join(plot_dir, f"{base_name}_3d.png")
        plt.savefig(output_3d, dpi=150, bbox_inches='tight')
        print(f"    Saved: {output_3d}")
        plt.close()
    else:
        print("\n[5/5] Skipping visualizations (use -plot flag to generate plots. Note that it may take a long time and may not be useful for large cases)")
    
    print("\n" + "="*60)
    print("Evaluation complete!")
    print("="*60)
    
    # Return non-zero exit code if there are errors
    if not validity_result['all_valid'] or not conn_result['all_connected']:
        sys.exit(1)


if __name__ == "__main__":
    main()

