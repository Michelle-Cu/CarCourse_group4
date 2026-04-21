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

from hm10_esp32_bridge import HM10ESP32Bridge

logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)

log = logging.getLogger(__name__)

# TODO : Fill in the following information
TEAM_NAME = "陽明山車神"
SERVER_URL = "http://carcar.ntuee.org/scoreboard"
MAZE_FILE = "data/cross.csv"
BT_PORT = "" # remember to fill in!




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


def main(mode: int, bt_port: str, team_name: str, server_url: str, maze_file: str):
    # maze = Maze(maze_file)
    BASE = Path(__file__).resolve().parent
    maze = Maze(str(BASE / "data" / "cross.csv"))

    # new code 
    if mode == "0":
        log.info("Mode 0: For treasure-hunting")
        # TODO

        point = ScoreboardServer(team_name, server_url)
        print("Game Started.")
        bridge = HM10ESP32Bridge(port=bt_port)
        
        current = maze.get_start_point()
        submitted = set()
        action_queue = []

        while True:
            messages = bridge.listen()
            for msg in messages:
                msg = msg.strip()
                if not msg:
                    continue
                
                if msg == "reqNxtMove":
                    # if queue empty, compute new BFS path
                    if not action_queue:
                        path = maze.BFS(current)
                        if path is None:
                            if not action_queue or action_queue[-1] != Action.HALT:
                                action_queue.append(Action.HALT)
                            log.info("Maze fully explored! Adding HALT to queue.")
                        else:
                            actions = maze.getActions(path)
                            action_queue = list(actions)
                            current = path[-1]  # update current to end of path
                            log.info(f"New path: {[n.get_index() for n in path]}")
                            log.info(f"Actions: {maze.actions_to_str(list(actions))}")
                    
                    if action_queue:
                        # pop next action from queue
                        next_action = action_queue.pop(0)
                        bridge.send(f"nxtMove:{int(next_action)}")
                        log.info(f"Sent: nxtMove:{int(next_action)}")
                    else:
                        bridge.send("nxtMove:5")
                        log.info("Sent: nxtMove:5 (Queue empty)")

                elif msg.startswith("RFID:"):
                    uid_str = msg[5:].strip().upper()
                    # Basic validation: must be 8 hex characters
                    if len(uid_str) == 8 and all(c in "0123456789ABCDEF" for c in uid_str):
                        if uid_str not in submitted:
                            score, time_remaining = point.add_UID(uid_str)
                            print(f"Added {score} Points at {time_remaining} seconds left.")
                            submitted.add(uid_str)
                    else:
                        log.warning(f"⚠️ Received malformed RFID UID: '{uid_str}'")

    elif mode == "1":
        log.info("Mode 1: Self-testing mode.")

        # ── uncomment ONE block at a time ──

        # 1. server connection test (no BT needed)
        point = ScoreboardServer(team_name, server_url)
        print("Game Started.")
        while True:
            time.sleep(1)

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
        # actions = maze.getActions(path)
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
