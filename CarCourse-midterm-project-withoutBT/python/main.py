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

logging.basicConfig(
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s", level=logging.INFO
)

log = logging.getLogger(__name__)

# TODO : Fill in the following information
TEAM_NAME = "Group4"
SERVER_URL = "http://carcar.ntuee.org/scoreboard"
MAZE_FILE = "data/small_maze.csv"
BT_PORT = ""


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
    maze = Maze(str(BASE / "data" / "small_maze.csv"))

    # point = ScoreboardServer(team_name, server_url)
    print("Game Started")
    point = ScoreboardFake(team_name, "data/fakeUID.csv") # for local testing

    ### Bluetooth connection haven't been implemented yet, we will update ASAP ###
    # interface = BTInterface(port=bt_port)
    # TODO : Initialize necessary variables

    if mode == "0":
        log.info("Mode 0: For treasure-hunting")
        # TODO : for treasure-hunting, which encourages you to hunt as many scores as possible

    elif mode == "1":
        log.info("Mode 1: Self-testing mode.")
        # TODO: You can write your code to test specific function.

        # Testing if our team name appears on the server

        # score, time_remaining = point.add_UID("10BA617E")
        # log.info(f"Score: {score}, Time remaining: {time_remaining}")

        # Keep the loop running so I can view our group name on the server
        # log.info("Keeping connection alive... press Ctrl+C to stop")
        # while True:
        #     time.sleep(1)


        # test BFS

        # current = maze.get_start_point()
        # path = maze.BFS(current)
        # print("Path 1:", [n.get_index() for n in path])
        
        # path2 = maze.BFS(path[-1])
        # print("Path 2:", [n.get_index() for n in path2])

        # # test UID submission
        # score, _ = point.add_UID("10BA617E")
        # log.info(f"Score: {score}")

        # test BFS_2
        # path = maze.BFS_2(maze.node_dict[1], maze.node_dict[6])
        # print("BFS_2 path:", [n.get_index() for n in path])
        # # should print [1, 2, 3, 5, 6]

        # actions = maze.getActions(path)
        # print(maze.actions_to_str(actions))

        # simulate full exploration

        current = maze.get_start_point()
        NODE_TO_UID = {
            1: "50335F7E",   # 30 pts
            4: "10BA617E",   # 10 pts
            6: "84EAB017",   # 20 pts
        }

        # tracks nodes that have already been submitted
        submitted = set()

        # check if starting node has a UID
        if current.get_index() in NODE_TO_UID:
            uid = NODE_TO_UID[current.get_index()]
            score, _ = point.add_UID(uid)
            submitted.add(current.get_index())
            log.info(f"Node {current.get_index()}: got {score} pts")

        # then explore
        while True:
            path = maze.BFS(current)
            if path is None:
                break
            for node in path:
                if node.get_index() in NODE_TO_UID and node.get_index() not in submitted:
                    uid = NODE_TO_UID[node.get_index()]
                    score, _ = point.add_UID(uid)
                    submitted.add(node.get_index())
                    log.info(f"Node {node.get_index()}: got {score} pts")
            current = path[-1]
        log.info(f"Total score: {point.get_current_score()}")

    else:
        log.error("Invalid mode")
        sys.exit(1)


if __name__ == "__main__":
    args = parse_args()
    main(**vars(args))
