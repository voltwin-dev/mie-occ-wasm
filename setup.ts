import { execSync, ExecSyncOptionsWithBufferEncoding } from "child_process";
import path from "path";
import fs from "fs";

const ROOT_DIR = path.resolve(__dirname);
const BUILD_DIR = path.join(ROOT_DIR, "build");

const EMSDK_DIR = path.join(BUILD_DIR, "emsdk");
const EMSDK_VERSION = "4.0.15";

const OCCT_DIR = path.join(BUILD_DIR, "occt");

function execAsync(command: string, options?: ExecSyncOptionsWithBufferEncoding): Promise<void> {
    return new Promise((resolve, reject) => {
        try {
            execSync(command, options);
            resolve();
        } catch (error) {
            reject(error);
        }
    });
}

interface InstallTask {
    name: string;
    repository: string;
    tag: string;
    targetDir: string;
    commands: string[];
}

const tasks: InstallTask[] = [
    {
        name: "emsdk",
        repository: "https://github.com/emscripten-core/emsdk.git",
        tag: EMSDK_VERSION,
        targetDir: EMSDK_DIR,
        commands: [
            `${EMSDK_DIR}/emsdk install ${EMSDK_VERSION}`,
            `${EMSDK_DIR}/emsdk activate --embedded ${EMSDK_VERSION}`,
        ]
    },
    {
        name: "Open CASCADE Technology",
        repository: "https://github.com/Open-Cascade-SAS/OCCT.git",
        tag: "V7_9_1",
        targetDir: OCCT_DIR,
        commands: []
    }
];

async function run() {
    for (const task of tasks) {
        if (!fs.existsSync(task.targetDir)) {
            console.log(`Cloning ${task.name}...`);
            await execAsync(`git clone --depth 1 --branch ${task.tag} ${task.repository} ${task.targetDir}`);
        }
        for (const command of task.commands) {
            console.log(`Running command for ${task.name}: ${command}`);
            await execAsync(command, { stdio: "inherit" });
        }
        console.log(`${task.name} setup completed.`);
    }
}

run().catch(error => {
    console.error("Setup failed:", error);
    process.exit(1);
});
