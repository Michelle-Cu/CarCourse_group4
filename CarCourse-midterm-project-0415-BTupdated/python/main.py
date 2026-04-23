import argparse
import logging
import os
import sys
import time

import numpy as np
import pandas
# from BTinterface import BTInterface
from maze import Action, Maze
from score import ScoreboardServer, ScoreboardFake
from pathlib import Path

from bluetooth import setup_bluetooth

logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)

log = logging.getLogger(__name__)

# TODO : Fill in the following information
TEAM_NAME = "陽明山車神"
SERVER_URL = "http://carcar.ntuee.org/scoreboard"
MAZE_FILE = "data/cross.csv"
BT_PORT = "COM5" # remember to fill in!
EXPECTED_NAME = "BT4"

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", help="0: treasure-hunting, 1: self-testing", type=str)
    parser.add_argument("--maze-file", default=MAZE_FILE, help="Maze file", type=str)
    parser.add_argument("--bt-port", default=BT_PORT, help="Bluetooth port", type=str)
    parser.add_argument(
        "--team-name", default=TEAM_NAME, help="Your team name", type=str
    )
    parser.add_argument("--server-url", default=SERVER_URL, help="Server URL", type=str)
    return parser.parse_args()


def main(mode: str, bt_port: str, team_name: str, server_url: str, maze_file: str):
    BASE = Path(__file__).resolve().parent
    # Check if maze_file is a relative path to BASE/data or an absolute path
    if os.path.exists(os.path.join(BASE, "data", maze_file)):
        maze_path = str(BASE / "data" / maze_file)
    elif os.path.exists(os.path.join(BASE, maze_file)):
        maze_path = str(BASE / maze_file)
    else:
        maze_path = maze_file
        
    log.info(f"Loading maze from: {maze_path}")
    maze = Maze(maze_path)

    # new code 
    if mode == "0":
        log.info("Mode 0: For treasure-hunting")
        # TODO

        bridge = setup_bluetooth(bt_port, EXPECTED_NAME, log)
        if bridge is None:
            sys.exit(1)

        point = ScoreboardServer(team_name, server_url)

        # --- DEBUG: Print full BFS action sequence ---
        debug_current = maze.get_start_point()
        debug_actions = []
        debug_car_dir = None
        # Save actual exploration state
        original_explored = maze.explored.copy()
        maze.explored = set()
        
        while True:
            p = maze.BFS(debug_current)
            if p is None: break
            
            if debug_car_dir is None and len(p) >= 2:
                debug_car_dir = p[0].get_direction(p[1])
            
            if len(p) >= 2:
                for i in range(len(p) - 1):
                    act, debug_car_dir = maze.getAction(debug_car_dir, p[i], p[i+1])
                    debug_actions.append(int(act))
            debug_current = p[-1]
        
        log.info(f"DEBUG: Full BFS Action Sequence: {debug_actions}")
        # Restore actual exploration state
        maze.explored = original_explored
        # ---------------------------------
        
        current = maze.get_start_point()
        submitted = set()
        action_queue = []
        waiting_for_ack = False
        current_pending_batch = ""
        game_started = False
        
        # New synchronization variables
        moves_done_python = 0

        # Track car direction across multiple BFS calls
        car_dir = None

        # Trigger BTConnected on Arduino
        bridge.send("check")

        while True:
            messages = bridge.listen()
            for msg in messages:
                msg = msg.strip()
                if not msg:
                    continue

                # Robust Status Sync: S:done,m1,m2,m3
                if msg.startswith("S:"):
                    try:
                        parts = msg[2:].split(",")
                        if len(parts) < 4:
                            # If it's the old S:done,count format (2 parts), we can still use it for done-sync
                            if len(parts) == 2:
                                arduino_done = int(parts[0])
                                diff = arduino_done - moves_done_python
                                if diff > 0:
                                    for _ in range(diff):
                                        if action_queue:
                                            popped = action_queue.pop(0)
                                            log.info(f"🚗 Status(old) confirm: Move done. Popped {Action(popped).name}.")
                                    moves_done_python = arduino_done
                            continue
                            
                        arduino_done = int(parts[0])
                        arduino_moves = [int(x) for x in parts[1:4]] # m1, m2, m3

                        # 1. Sync finished moves
                        diff = arduino_done - moves_done_python
                        if diff > 0:
                            for _ in range(diff):
                                if action_queue:
                                    popped = action_queue.pop(0)
                                    log.info(f"🚗 Status confirm: Move done. Popped {Action(popped).name}. Remaining: {len(action_queue)}")
                            moves_done_python = arduino_done

                        # 2. Verify buffer content and refill if low
                        # We compare the first 3 moves in Python's queue with Arduino's reported moves
                        needs_resend = False
                        arduino_count = 0
                        for i in range(3):
                            if i < len(action_queue):
                                if arduino_moves[i] != int(action_queue[i]):
                                    needs_resend = True
                                if arduino_moves[i] != 0:
                                    arduino_count += 1
                            elif arduino_moves[i] != 0:
                                # Arduino has moves Python doesn't know about? (Shouldn't happen)
                                needs_resend = True

                        if needs_resend or arduino_count < 3:
                            # If queue is low, run BFS to get more
                            while len(action_queue) < 3:
                                bfs_start_node = current
                                
                                path = maze.BFS(bfs_start_node)
                                if path is None:
                                    # Pad with 3 HALTs to ensure robot stops correctly
                                    while len(action_queue) < 3 or action_queue[-3:] != [Action.HALT]*3:
                                        action_queue.append(Action.HALT)
                                    break
                                
                                if car_dir is None and len(path) >= 2:
                                    car_dir = path[0].get_direction(path[1])

                                actions_batch = []
                                if len(path) >= 2:
                                    for i in range(len(path) - 1):
                                        action, car_dir = maze.getAction(car_dir, path[i], path[i+1])
                                        actions_batch.append(action)

                                action_queue.extend(actions_batch)
                                current = path[-1]
                            
                            # Send moves starting from the absolute index matching the start of Python's queue
                            start_idx = moves_done_python + 1
                            moves_to_send = action_queue[:3]
                            batch_str = ",".join(map(str, [int(a) for a in moves_to_send]))
                            bridge.send(f"nxtMove:{start_idx},{batch_str}")
                            log.info(f"Sent Sync/Fix Batch: nxtMove:{start_idx},{batch_str}")
                        
                        # 3. Handle game start prompt
                        if not game_started and arduino_count > 0:
                            print("\n" + "="*40)
                            print("FIRST MOVES RECEIVED BY ARDUINO")
                            print("Robot is ready and waiting.")
                            print("="*40)
                            input("Press Enter to START the game (starts server and bot)... ")
                            
                            log.info("Starting game on scoreboard server and bot...")
                            point.start_game()
                            bridge.send("gameStart")
                            game_started = True
                            print("Game Started.")

                    except (ValueError, IndexError) as e:
                        log.error(f"⚠️ Malformed Status received: {msg} ({e})")

                elif msg == "reqFirstMove":
                    log.info("Arduino restarted — resetting action queue and exploration")
                    action_queue = []
                    maze.explored = set()
                    current = maze.get_start_point()
                    car_dir = None
                    waiting_for_ack = False
                    current_pending_batch = ""
                    moves_done_python = 0
                    # Gather moves
                    while len(action_queue) < 3:
                        path = maze.BFS(current)
                        if path is None:
                            if not action_queue or action_queue[-1] != Action.HALT:
                                action_queue.append(Action.HALT)
                            break
                        
                        if car_dir is None and len(path) >= 2:
                            car_dir = path[0].get_direction(path[1])
                            
                        actions_batch = []
                        if len(path) >= 2:
                            for i in range(len(path) - 1):
                                action, car_dir = maze.getAction(car_dir, path[i], path[i+1])
                                actions_batch.append(action)
                            
                        action_queue.extend(actions_batch)
                        current = path[-1]
                    
                    if not action_queue:
                        bridge.send("nxtMove:1,5")
                    else:
                        moves_to_send = action_queue[:3]
                        current_pending_batch = ",".join(map(str, [int(a) for a in moves_to_send]))
                        bridge.send(f"nxtMove:1,{current_pending_batch}")
                        log.info(f"Sent First Batch: nxtMove:1,{current_pending_batch}")

                elif msg == "reqNxtMove":
                    # This is now handled by the periodic status sync, 
                    # but we keep it for backward compatibility if needed.
                    pass

                elif msg.startswith("moveDone:"):
                    # Fast-path move completion
                    try:
                        finished_move = int(msg.split(":")[1].strip())
                        if action_queue:
                            # We check if we already processed this via status sync
                            # If not, we can pop here for lower latency
                            pass 
                    except (ValueError, IndexError):
                        log.error(f"⚠️ Malformed moveDone received: {msg}")

                elif msg.startswith("RFID:"):
                    uid_str = msg[5:].strip().upper()
                    # Basic validation: must be 8 hex characters
                    if len(uid_str) == 8 and all(c in "0123456789ABCDEF" for c in uid_str):
                        if uid_str not in submitted:
                            score, time_remaining = point.add_UID(uid_str)
                            print(f"Added {score} Points at {time_remaining} seconds left.")
                            submitted.add(uid_str)
                        bridge.send(f"rfidAck:{uid_str.lower()}")
                    else:
                        log.warning(f"⚠️ Received malformed RFID UID: '{uid_str}'. Requesting resend...")
                        bridge.send("rfidResend")
                log.info(msg)
            
            time.sleep(0.05)


    elif mode == "1":
        log.info("Mode 1: Shortest path moving (BFS_2) with Mode 0 protocol.")

        START_NODE = 1
        END_NODE = 6 # Modify as needed

        bridge = setup_bluetooth(bt_port, EXPECTED_NAME, log)
        if bridge is None:
            sys.exit(1)

        point = ScoreboardServer(team_name, server_url)
        
        # Pre-calculate path
        path = maze.BFS_2(maze.node_dict[START_NODE], maze.node_dict[END_NODE])
        if path is None:
            log.error(f"No path found between {START_NODE} and {END_NODE}")
            sys.exit(1)
        
        log.info(f"Shortest path from {START_NODE} to {END_NODE}: {[n.get_index() for n in path]}")
        actions, _ = maze.getActions(path)
        original_action_queue = list(actions)
        # Pad with three HALTs
        original_action_queue.extend([Action.HALT, Action.HALT, Action.HALT])

        submitted = set()
        action_queue = list(original_action_queue)
        waiting_for_ack = False
        current_pending_batch = ""
        game_started = False
        
        # New synchronization variables
        moves_done_python = 0

        # Trigger BTConnected on Arduino
        bridge.send("check")

        while True:
            messages = bridge.listen()
            for msg in messages:
                msg = msg.strip()
                if not msg:
                    continue
                
                # Robust Status Sync: S:done,m1,m2,m3
                if msg.startswith("S:"):
                    try:
                        parts = msg[2:].split(",")
                        if len(parts) < 4:
                            # If it's the old S:done,count format (2 parts), we can still use it for done-sync
                            if len(parts) == 2:
                                arduino_done = int(parts[0])
                                diff = arduino_done - moves_done_python
                                if diff > 0:
                                    for _ in range(diff):
                                        if action_queue:
                                            popped = action_queue.pop(0)
                                            log.info(f"🚗 Status(old) confirm: Move done. Popped {Action(popped).name}.")
                                    moves_done_python = arduino_done
                            continue
                            
                        arduino_done = int(parts[0])
                        arduino_moves = [int(x) for x in parts[1:4]] # m1, m2, m3

                        # 1. Sync finished moves
                        diff = arduino_done - moves_done_python
                        if diff > 0:
                            for _ in range(diff):
                                if action_queue:
                                    popped = action_queue.pop(0)
                                    log.info(f"🚗 Status confirm: Move done. Popped {Action(popped).name}. Remaining: {len(action_queue)}")
                            moves_done_python = arduino_done

                        # 2. Verify buffer content and refill if low
                        needs_resend = False
                        arduino_count = 0
                        for i in range(3):
                            if i < len(action_queue):
                                if arduino_moves[i] != int(action_queue[i]):
                                    needs_resend = True
                                if arduino_moves[i] != 0:
                                    arduino_count += 1
                            elif arduino_moves[i] != 0:
                                needs_resend = True

                        if needs_resend or arduino_count < 3:
                            # Send moves starting from the absolute index matching the start of Python's queue
                            if action_queue:
                                start_idx = moves_done_python + 1
                                moves_to_send = action_queue[:3]
                                batch_str = ",".join(map(str, [int(a) for a in moves_to_send]))
                                bridge.send(f"nxtMove:{start_idx},{batch_str}")
                                log.info(f"Sent Sync/Fix Batch (Mode 1): nxtMove:{start_idx},{batch_str}")
                        
                        # 3. Handle game start prompt
                        if not game_started and arduino_count > 0:
                            print("\n" + "="*40)
                            print("FIRST MOVES RECEIVED BY ARDUINO")
                            print("Robot is ready and waiting.")
                            print("="*40)
                            input("Press Enter to START the shortest path traversal... ")
                            
                            log.info("Starting game on scoreboard server and bot...")
                            point.start_game()
                            bridge.send("gameStart")
                            game_started = True
                            print("Game Started.")

                    except (ValueError, IndexError) as e:
                        log.error(f"⚠️ Malformed Status received: {msg} ({e})")

                elif msg == "reqFirstMove":
                    log.info("Arduino restarted — resetting shortest path action queue")
                    action_queue = list(original_action_queue)
                    waiting_for_ack = False
                    current_pending_batch = ""
                    moves_done_python = 0
                    
                    if not action_queue:
                        bridge.send("nxtMove:1,5")
                    else:
                        moves_to_send = action_queue[:3]
                        current_pending_batch = ",".join(map(str, [int(a) for a in moves_to_send]))
                        bridge.send(f"nxtMove:1,{current_pending_batch}")
                        log.info(f"Sent First Batch: nxtMove:1,{current_pending_batch}")

                elif msg == "reqNxtMove":
                    # Handled by status sync
                    pass

                elif msg.startswith("nxtMoveReceived:"):
                    log.info(f"✅ Arduino ACK: {msg}")

                elif msg.startswith("moveDone:"):
                    # Optional fast-path completion
                    pass

                elif msg.startswith("RFID:"):
                    uid_str = msg[5:].strip().upper()
                    if len(uid_str) == 8 and all(c in "0123456789ABCDEF" for c in uid_str):
                        if uid_str not in submitted:
                            score, time_remaining = point.add_UID(uid_str)
                            print(f"Added {score} Points at {time_remaining} seconds left.")
                            submitted.add(uid_str)
                        bridge.send(f"rfidAck:{uid_str.lower()}")
                    else:
                        log.warning(f"⚠️ Received malformed RFID UID: '{uid_str}'. Requesting resend...")
                        bridge.send("rfidResend")
            
            time.sleep(0.05)


    elif mode == "2":
        log.info("Mode 2: Self-testing mode (Original Template).")

        # ── uncomment ONE block at a time ──

        # 1. server connection test (no BT needed)
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # while True:
        #     time.sleep(1)

        # 2. local BFS test (no BT needed)
        # point = ScoreboardFake(team_name, str(BASE / "data" / "fakeUID.csv"))
        # current = maze.get_start_point()
        # path = maze.BFS(current)
        # print("Path 1:", [n.get_index() for n in path])
        # path2 = maze.BFS(path[-1])
        # print("Path 2:", [n.get_index() for n in path2])

        # 3. BFS_2 shortest path + car movement (needs BT)
        # ── give it a start and end node, car walks the shortest path ──
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # bridge = HM10ESP32Bridge(port=bt_port)
        
        # START_NODE = 1
        # END_NODE = 5
        # path = maze.BFS_2(maze.node_dict[START_NODE], maze.node_dict[END_NODE])
        # print(f"Path: {[n.get_index() for n in path]}")
        # actions, car_dir = maze.getActions(path)
        # action_queue = list(actions)
        # log.info(f"Action sequence: {maze.actions_to_str(actions)}")
        
        # current_node = maze.node_dict[START_NODE]
        # path_index = 1  # tracks which node we're heading to
        
        # log.info("Running BFS_2 path... press Ctrl+C to stop")
        # while True:
        #     messages = bridge.listen()
        #     for msg in messages:
        #         msg = msg.strip()
        #         if not msg:
        #             continue
        
        #          # car asking for next move
        #         if msg == "reqNxtMove":
        #             if action_queue:
        #                 next_action = action_queue.pop(0)
        #                 bridge.send(f"nxtMove:{int(next_action)}")
        #                 log.info(f"Sent: nxtMove:{int(next_action)}")
        #                 if path_index < len(path):
        #                     current_node = path[path_index]
        #                     path_index += 1
        #             else:
        #                 bridge.send("nxtMove:5")  # HALT, done
        #                 log.info("Path complete! Sending HALT.")
        
        #         # car sending RFID
        #         elif msg.startswith("RFID:"):
        #             uid_str = msg[5:].upper()
        #             log.info(f"Received UID: {uid_str}")
        #             score, time_remaining = point.add_UID(uid_str)
        #             print(f"Added {score} Points at {time_remaining} seconds left.")
        #
        # 4. GOAL 2: RFID scan only, no movement (needs BT)
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # bridge = HM10ESP32Bridge(port=bt_port)
        # log.info("Waiting for RFID scans... press Ctrl+C to stop")
        # while True:
        #     messages = bridge.listen()
        #     for msg in messages:
        #         msg = msg.strip()
        #         if not msg:
        #             continue
        #         if msg.startswith("RFID:"):
        #             uid_str = msg[5:].upper()
        #             log.info(f"Received UID: {uid_str}")
        #             score, time_remaining = point.add_UID(uid_str)
        #             print(f"Added {score} Points at {time_remaining} seconds left.")
    
        # 5. fake full exploration (no BT needed)
        # point = ScoreboardFake(team_name, str(BASE / "data" / "fakeUID.csv"))
        # current = maze.get_start_point()
        # NODE_TO_UID = {
        #     1: "10BA617E",   # 10 pts  ← starting dead-end
        #     3: "84EAB017",   # 20 pts  ← East dead-end
        #     4: "50335F7E",   # 30 pts  ← West dead-end
        #     5: "353D0AD6",   # 40 pts  ← South dead-end
        # }
        # submitted = set()
        # if current.get_index() in NODE_TO_UID:
        #     uid = NODE_TO_UID[current.get_index()]
        #     score, _ = point.add_UID(uid)
        #     submitted.add(current.get_index())
        #     log.info(f"Node {current.get_index()}: got {score} pts")
        # while True:
        #     path = maze.BFS(current)
        #     if path is None:
        #         break
        #     for node in path:
        #         if node.get_index() in NODE_TO_UID and node.get_index() not in submitted:
        #             uid = NODE_TO_UID[node.get_index()]
        #             score, _ = point.add_UID(uid)
        #             submitted.add(node.get_index())
        #             log.info(f"Node {node.get_index()}: got {score} pts")
        #     current = path[-1]
        # log.info(f"Total score: {point.get_current_score()}")

    

    else:
        log.error("Invalid mode")
        sys.exit(1)


if __name__ == "__main__":
    args = parse_args()
    main(**vars(args))



# uncomment to test full map traversal (nodes visited and direction commands)

# if __name__ == "__main__":
#     m = Maze(str(Path(__file__).resolve().parent / "data" / "medium_maze.csv"))
    
#     current = m.get_start_point()
#     all_nodes_visited = [m.get_start_point().get_index()]
#     all_actions = []
#     step = 1
#     car_dir = None

#     action_names = {1: "ADVANCE", 2: "TURN_LEFT", 3: "U_TURN", 4: "TURN_RIGHT", 5: "HALT"}

#     print("=== Full Map Traversal Simulation ===\n")

#     while True:
#         path = m.BFS(current)
#         if path is None:
#             print("All dead-ends explored! Traversal complete.")
#             break

#         actions, car_dir = m.getActions(path, initial_dir=car_dir)
#         action_str = m.actions_to_str(actions)
#         action_nums = [int(a) for a in actions]

#         print(f"--- Path {step} ---")
#         print(f"Node path:    {' → '.join(str(n.get_index()) for n in path)}")
#         print(f"Actions (str): {action_str}")           # e.g. "frf"
#         print(f"Actions (num): {action_nums}")           # e.g. [1, 4, 1]
#         print()

#         for i, (node, action) in enumerate(zip(path, actions)):
#             print(f"  Step {i+1}: at node {node.get_index()} → {action_names[int(action)]} ({int(action)})")
#         print(f"  Step {len(path)}: arrive at node {path[-1].get_index()}")
#         print()

#         all_nodes_visited.extend(n.get_index() for n in path[1:])
#         all_actions.extend(actions)
#         # all_actions.append(Action.U_TURN)
#         current = path[-1]
#         step += 1

#     print("=== Summary ===")
#     print(f"Nodes visited:      {all_nodes_visited}")
#     print(f"Full actions (str): {m.actions_to_str(all_actions)}")
#     print(f"Full actions (num): {[int(a) for a in all_actions]}")
#     print(f"Total steps:        {len(all_actions)}")
        
    # time.sleep(0.05)


    # elif mode == "2":
    #     log.info("Mode 2: Self-testing mode (Original Template).")

        # ── uncomment ONE block at a time ──

        # 1. server connection test (no BT needed)
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # while True:
        #     time.sleep(1)

        # 2. local BFS test (no BT needed)
        # point = ScoreboardFake(team_name, str(BASE / "data" / "fakeUID.csv"))
        # current = maze.get_start_point()
        # path = maze.BFS(current)
        # print("Path 1:", [n.get_index() for n in path])
        # path2 = maze.BFS(path[-1])
        # print("Path 2:", [n.get_index() for n in path2])

        # 3. BFS_2 shortest path + car movement (needs BT)
        # ── give it a start and end node, car walks the shortest path ──
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # bridge = HM10ESP32Bridge(port=bt_port)
        
        # START_NODE = 1
        # END_NODE = 5
        # path = maze.BFS_2(maze.node_dict[START_NODE], maze.node_dict[END_NODE])
        # print(f"Path: {[n.get_index() for n in path]}")
        # actions, car_dir = maze.getActions(path)
        # action_queue = list(actions)
        # log.info(f"Action sequence: {maze.actions_to_str(actions)}")
        
        # current_node = maze.node_dict[START_NODE]
        # path_index = 1  # tracks which node we're heading to
        
        # log.info("Running BFS_2 path... press Ctrl+C to stop")
        # while True:
        #     messages = bridge.listen()
        #     for msg in messages:
        #         msg = msg.strip()
        #         if not msg:
        #             continue
        
        #          # car asking for next move
        #         if msg == "reqNxtMove":
        #             if action_queue:
        #                 next_action = action_queue.pop(0)
        #                 bridge.send(f"nxtMove:{int(next_action)}")
        #                 log.info(f"Sent: nxtMove:{int(next_action)}")
        #                 if path_index < len(path):
        #                     current_node = path[path_index]
        #                     path_index += 1
        #             else:
        #                 bridge.send("nxtMove:5")  # HALT, done
        #                 log.info("Path complete! Sending HALT.")
        
        #         # car sending RFID
        #         elif msg.startswith("RFID:"):
        #             uid_str = msg[5:].upper()
        #             log.info(f"Received UID: {uid_str}")
        #             score, time_remaining = point.add_UID(uid_str)
        #             print(f"Added {score} Points at {time_remaining} seconds left.")
        #
        # 4. GOAL 2: RFID scan only, no movement (needs BT)
        # point = ScoreboardServer(team_name, server_url)
        # print("Game Started.")
        # bridge = HM10ESP32Bridge(port=bt_port)
        # log.info("Waiting for RFID scans... press Ctrl+C to stop")
        # while True:
        #     messages = bridge.listen()
        #     for msg in messages:
        #         msg = msg.strip()
        #         if not msg:
        #             continue
        #         if msg.startswith("RFID:"):
        #             uid_str = msg[5:].upper()
        #             log.info(f"Received UID: {uid_str}")
        #             score, time_remaining = point.add_UID(uid_str)
        #             print(f"Added {score} Points at {time_remaining} seconds left.")
    
        # 5. fake full exploration (no BT needed)
        # point = ScoreboardFake(team_name, str(BASE / "data" / "fakeUID.csv"))
        # current = maze.get_start_point()
        # NODE_TO_UID = {
        #     1: "10BA617E",   # 10 pts  ← starting dead-end
        #     3: "84EAB017",   # 20 pts  ← East dead-end
        #     4: "50335F7E",   # 30 pts  ← West dead-end
        #     5: "353D0AD6",   # 40 pts  ← South dead-end
        # }
        # submitted = set()
        # if current.get_index() in NODE_TO_UID:
        #     uid = NODE_TO_UID[current.get_index()]
        #     score, _ = point.add_UID(uid)
        #     submitted.add(current.get_index())
        #     log.info(f"Node {current.get_index()}: got {score} pts")
        # while True:
        #     path = maze.BFS(current)
        #     if path is None:
        #         break
        #     for node in path:
        #         if node.get_index() in NODE_TO_UID and node.get_index() not in submitted:
        #             uid = NODE_TO_UID[node.get_index()]
        #             score, _ = point.add_UID(uid)
        #             submitted.add(node.get_index())
        #             log.info(f"Node {node.get_index()}: got {score} pts")
        #     current = path[-1]
        # log.info(f"Total score: {point.get_current_score()}")

    

#     else:
#         log.error("Invalid mode")
#         sys.exit(1)


# if __name__ == "__main__":
#     args = parse_args()
#     main(**vars(args))



# uncomment to test full map traversal (nodes visited and direction commands)

# if __name__ == "__main__":
#     m = Maze(str(Path(__file__).resolve().parent / "data" / "medium_maze.csv"))
    
#     current = m.get_start_point()
#     all_nodes_visited = [m.get_start_point().get_index()]
#     all_actions = []
#     step = 1
#     car_dir = None

#     action_names = {1: "ADVANCE", 2: "TURN_LEFT", 3: "U_TURN", 4: "TURN_RIGHT", 5: "HALT"}

#     print("=== Full Map Traversal Simulation ===\n")

#     while True:
#         path = m.BFS(current)
#         if path is None:
#             print("All dead-ends explored! Traversal complete.")
#             break

#         actions, car_dir = m.getActions(path, initial_dir=car_dir)
#         action_str = m.actions_to_str(actions)
#         action_nums = [int(a) for a in actions]

#         print(f"--- Path {step} ---")
#         print(f"Node path:    {' → '.join(str(n.get_index()) for n in path)}")
#         print(f"Actions (str): {action_str}")           # e.g. "frf"
#         print(f"Actions (num): {action_nums}")           # e.g. [1, 4, 1]
#         print()

#         for i, (node, action) in enumerate(zip(path, actions)):
#             print(f"  Step {i+1}: at node {node.get_index()} → {action_names[int(action)]} ({int(action)})")
#         print(f"  Step {len(path)}: arrive at node {path[-1].get_index()}")
#         print()

#         all_nodes_visited.extend(n.get_index() for n in path[1:])
#         all_actions.extend(actions)
#         # all_actions.append(Action.U_TURN)
#         current = path[-1]
#         step += 1

#     print("=== Summary ===")
#     print(f"Nodes visited:      {all_nodes_visited}")
#     print(f"Full actions (str): {m.actions_to_str(all_actions)}")
#     print(f"Full actions (num): {[int(a) for a in all_actions]}")
#     print(f"Total steps:        {len(all_actions)}")