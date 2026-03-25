import { execSync, ExecSyncOptionsWithBufferEncoding } from "child_process";
import path from "path";
import fs from "fs";

const ROOT_DIR = path.resolve(__dirname);
const BUILD_DIR = path.join(ROOT_DIR, "build");
const TASK_TAGS_FILE = path.join(BUILD_DIR, ".task-tags.json");

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
    key: string;
    name: string;
    repository: string;
    tag: string;
    targetDir: string;
    commands: string[];
}

const tasks: InstallTask[] = [
    {
        key: "emsdk",
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
        key: "occt",
        name: "Open CASCADE Technology",
        repository: "https://github.com/Open-Cascade-SAS/OCCT.git",
        tag: "V7_9_1",
        targetDir: OCCT_DIR,
        commands: []
    }
];

function readTaskTags(): Record<string, string> {
    if (!fs.existsSync(TASK_TAGS_FILE)) {
        return {};
    }

    try {
        const raw = fs.readFileSync(TASK_TAGS_FILE, "utf8");
        const parsed = JSON.parse(raw) as unknown;
        if (parsed !== null && typeof parsed === "object") {
            return parsed as Record<string, string>;
        }
    } catch {
        // If the file is malformed, ignore it and regenerate.
    }

    return {};
}

function writeTaskTags(taskTags: Record<string, string>): void {
    fs.writeFileSync(TASK_TAGS_FILE, JSON.stringify(taskTags, null, 2));
}

async function run(): Promise<void> {
    fs.mkdirSync(BUILD_DIR, { recursive: true });

    const taskTags = readTaskTags();

    for (const task of tasks) {
        const savedTag = taskTags[task.key];
        if (fs.existsSync(task.targetDir)) {
            if (savedTag === undefined) {
                console.log(`No saved tag for ${task.name}. Deleting existing directory to ensure clean setup.`);
                fs.rmSync(task.targetDir, { recursive: true, force: true });
            } else if (savedTag !== task.tag) {
                console.log(`Tag changed for ${task.name}: ${savedTag} -> ${task.tag}. Reinstalling...`);
                fs.rmSync(task.targetDir, { recursive: true, force: true });
            }
        }

        if (!fs.existsSync(task.targetDir)) {
            console.log(`Cloning ${task.name}...`);
            await execAsync(`git clone --depth 1 --branch ${task.tag} ${task.repository} ${task.targetDir}`);
        }
        for (const command of task.commands) {
            console.log(`Running command for ${task.name}: ${command}`);
            await execAsync(command, { stdio: "inherit" });
        }

        taskTags[task.key] = task.tag;
        console.log(`${task.name} setup completed.`);
    }

    writeTaskTags(taskTags);
}

run().catch(error => {
    console.error("Setup failed:", error);
    process.exit(1);
});
